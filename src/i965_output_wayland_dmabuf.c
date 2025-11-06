/*
 * va_wayland_drm.c - Wayland/linux-dmabuf helpers
 *
 * Copyright (c) 2024 Simon Ser
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL INTEL AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "sysdeps.h"
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <xf86drm.h>
#include <va/va_backend.h>
#include <va/va_drmcommon.h>
#include <va/va_backend_wayland.h>
#include <wayland-client.h>
#include <linux-dmabuf-v1-client.h>
#include "i965_output_wayland_dmabuf.h"
#include "i965_drv_video.h"
#include "dso_utils.h"

typedef void (*VADisplayContextDestroyFunc)(VADriverContextP ctx);

typedef uint32_t (*wl_display_get_global_func)(struct wl_display *display,
											   const char *interface, uint32_t version);
typedef struct wl_event_queue *(*wl_display_create_queue_func)(struct wl_display *display);
typedef int (*wl_display_roundtrip_queue_func)(struct wl_display *display,
												struct wl_event_queue *queue);
typedef int (*wl_display_dispatch_queue_func)(struct wl_display *display, struct wl_event_queue *queue);
typedef void (*wl_event_queue_destroy_func)(struct wl_event_queue *queue);
typedef void *(*wl_proxy_create_wrapper_func)(struct wl_proxy *proxy);
typedef void(*wl_proxy_wrapper_destroy_func)(void *proxy);
typedef struct wl_proxy *(*wl_proxy_create_func)(struct wl_proxy *factory,
												 const struct wl_interface *interface);
typedef void (*wl_proxy_destroy_func)(struct wl_proxy *proxy);
typedef void (*wl_proxy_marshal_func)(struct wl_proxy *p, uint32_t opcode, ...);
typedef int (*wl_proxy_add_listener_func)(struct wl_proxy *proxy,
										  void (**implementation)(void), void *data);
typedef void (*wl_proxy_set_queue_func)(struct wl_proxy *proxy, struct wl_event_queue *queue);

struct wl_vtable {
	const struct wl_interface        *buffer_interface;
	const struct wl_interface        *registry_interface;
	wl_display_create_queue_func      display_create_queue;
	wl_display_roundtrip_queue_func   display_roundtrip_queue;
    wl_display_dispatch_queue_func    display_dispatch_queue;
	wl_event_queue_destroy_func       event_queue_destroy;
	wl_proxy_create_wrapper_func      proxy_create_wrapper;
	wl_proxy_wrapper_destroy_func     proxy_wrapper_destroy;
	wl_proxy_create_func              proxy_create;
	wl_proxy_destroy_func             proxy_destroy;
	wl_proxy_marshal_func             proxy_marshal;
	wl_proxy_add_listener_func        proxy_add_listener;
	wl_proxy_set_queue_func           proxy_set_queue;
};

struct intel_wayland_linux_dmabuf_context
{
    struct wl_vtable          vtable;

    struct dso_handle         *libwl_client_handle;
    struct wl_event_queue     *queue;
    struct wl_registry        *registry;

    bool                      has_linux_dmabuf;
    bool                      default_feedback_done;
};

/* These function are copied and adapted from the version inside
 * wayland-client-protocol.h
 */
static void *
registry_bind(
	struct wl_vtable          *wl_vtable,
	struct wl_registry        *wl_registry,
	uint32_t                   name,
	const struct wl_interface *interface,
	uint32_t                   version
)
{
	struct wl_proxy *id;

	id = wl_vtable->proxy_create((struct wl_proxy *) wl_registry,
								 interface);
	if (!id)
		return NULL;

	wl_vtable->proxy_marshal((struct wl_proxy *) wl_registry,
							 WL_REGISTRY_BIND, name, interface->name,
							 version, id);

	return (void *) id;
}

static int
registry_add_listener(
	struct wl_vtable                  *wl_vtable,
	struct wl_registry                *wl_registry,
	const struct wl_registry_listener *listener,
	void                              *data
)
{
	return wl_vtable->proxy_add_listener((struct wl_proxy *) wl_registry,
										 (void (**)(void)) listener, data);
}

static struct wl_registry *
display_get_registry(
	struct wl_vtable  *wl_vtable,
	struct wl_display *wl_display
)
{
	struct wl_proxy *callback;

	callback = wl_vtable->proxy_create((struct wl_proxy *) wl_display,
									   wl_vtable->registry_interface);
	if (!callback)
		return NULL;

	wl_vtable->proxy_marshal((struct wl_proxy *) wl_display,
							 WL_DISPLAY_GET_REGISTRY, callback);

	return (struct wl_registry *) callback;
}

static void
feedback_handle_done(
    void                                *data,
    struct zwp_linux_dmabuf_feedback_v1 *feedback
)
{
    VADriverContextP ctx = data;
	struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct intel_wayland_linux_dmabuf_context *wl_linux_dmabuf_ctx = i965->dmabuf_output;

    wl_linux_dmabuf_ctx->default_feedback_done = true;

    zwp_linux_dmabuf_feedback_v1_destroy(feedback);
}

static void
feedback_handle_format_table(
    void                                *data,
    struct zwp_linux_dmabuf_feedback_v1 *feedback,
    int                                  fd,
    uint32_t                             size
)
{
    close(fd);
}

/* XXX: replace with drmGetDeviceFromDevId() */
static drmDevice *
get_drm_device_from_dev_id(dev_t dev_id)
{
    uint32_t flags = 0;
    int devices_len, i, node_type;
    drmDevice *match = NULL, *dev;
    struct stat statbuf;

    devices_len = drmGetDevices2(flags, NULL, 0);
    if (devices_len < 0) {
        return NULL;
    }
    drmDevice **devices = calloc(devices_len, sizeof(*devices));
    if (devices == NULL) {
        return NULL;
    }
    devices_len = drmGetDevices2(flags, devices, devices_len);
    if (devices_len < 0) {
        free(devices);
        return NULL;
    }

    for (i = 0; i < devices_len; i++) {
        dev = devices[i];
        for (node_type = 0; node_type < DRM_NODE_MAX; node_type++) {
            if (!(dev->available_nodes & (1 << node_type)))
                continue;

            if (stat(dev->nodes[node_type], &statbuf) != 0) {
                continue;
            }

            if (statbuf.st_rdev == dev_id) {
                match = dev;
                break;
            }
        }
    }

    for (i = 0; i < devices_len; i++) {
        dev = devices[i];
        if (dev != match)
            drmFreeDevice(&dev);
    }
    free(devices);

    return match;
}

static void
feedback_handle_main_device(
    void                                *data,
    struct zwp_linux_dmabuf_feedback_v1 *feedback,
    struct wl_array                     *device_array
)
{
    dev_t dev_id;
    drmDevice *dev;
    const char *dev_path;
    VADriverContextP ctx = data;
    struct drm_state * const drm_state = ctx->drm_state;

    assert(device_array->size == sizeof(dev_id));
    memcpy(&dev_id, device_array->data, sizeof(dev_id));

    dev = get_drm_device_from_dev_id(dev_id);
    if (!dev) {
        return;
    }

    if (!(dev->available_nodes & (1 << DRM_NODE_RENDER)))
        goto end;

    dev_path = dev->nodes[DRM_NODE_RENDER];
    drm_state->fd = open(dev_path, O_RDWR | O_CLOEXEC);
    if (drm_state->fd < 0) {
        goto end;
    }

    drm_state->auth_type = VA_DRM_AUTH_CUSTOM;

end:
    drmFreeDevice(&dev);
}

static void
feedback_handle_tranche_done(
    void                                *data,
    struct zwp_linux_dmabuf_feedback_v1 *feedback
)
{
}

static void
feedback_handle_tranche_target_device(
    void                                *data,
    struct zwp_linux_dmabuf_feedback_v1 *feedback,
    struct wl_array                     *device_array
)
{
}

static void
feedback_handle_tranche_formats(
    void                                *data,
    struct zwp_linux_dmabuf_feedback_v1 *feedback,
    struct wl_array                     *indices_array
)
{
}

static void
feedback_handle_tranche_flags(
    void                                *data,
    struct zwp_linux_dmabuf_feedback_v1 *feedback,
    uint32_t                             flags
)
{
}

static const struct zwp_linux_dmabuf_feedback_v1_listener feedback_listener = {
    .done = feedback_handle_done,
    .format_table = feedback_handle_format_table,
    .main_device = feedback_handle_main_device,
    .tranche_done = feedback_handle_tranche_done,
    .tranche_target_device = feedback_handle_tranche_target_device,
    .tranche_formats = feedback_handle_tranche_formats,
    .tranche_flags = feedback_handle_tranche_flags,
};

static void
registry_handle_global(
    void               *data,
    struct wl_registry *registry,
    uint32_t            name,
    const char         *interface,
    uint32_t            version
)
{
    VADriverContextP ctx = data;
	struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct intel_wayland_linux_dmabuf_context *wl_linux_dmabuf_ctx = i965->dmabuf_output;
    struct wl_vtable * const wl_vtable = &wl_linux_dmabuf_ctx->vtable;
    struct zwp_linux_dmabuf_v1 *linux_dmabuf;
    struct zwp_linux_dmabuf_feedback_v1 *feedback;

    if (strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0 &&
        version >= 4) {
        wl_linux_dmabuf_ctx->has_linux_dmabuf = true;
        linux_dmabuf =
            registry_bind(wl_vtable, registry, name, &zwp_linux_dmabuf_v1_interface, 4);
        feedback = zwp_linux_dmabuf_v1_get_default_feedback(linux_dmabuf);
        zwp_linux_dmabuf_feedback_v1_add_listener(feedback, &feedback_listener, data);
        zwp_linux_dmabuf_v1_destroy(linux_dmabuf);
    }
}

static void
registry_handle_global_remove(
    void               *data,
    struct wl_registry *registry,
    uint32_t            name
)
{
}

static const struct wl_registry_listener registry_listener = {
    .global        = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

bool
i965_wayland_linux_dmabuf_create(VADriverContextP ctx)
{
    bool result = false;
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct VADriverVTableWayland *vtable = ctx->vtable_wayland;
    struct intel_wayland_linux_dmabuf_context *wl_linux_dmabuf_ctx;
    struct drm_state *drm_state;
    struct dso_handle *dso_handle;
    struct wl_display *display = NULL;

    wl_linux_dmabuf_ctx = calloc(1, sizeof(*wl_linux_dmabuf_ctx));
    if (!wl_linux_dmabuf_ctx) {
        goto end;
    }

    i965->dmabuf_output = wl_linux_dmabuf_ctx;
    
    struct wl_vtable *wl_vtable = &wl_linux_dmabuf_ctx->vtable;
    static const struct dso_symbol libwl_client_symbols[] = {
		{
			"wl_buffer_interface",
			offsetof(struct wl_vtable, buffer_interface)
		},
		{
			"wl_registry_interface",
			offsetof(struct wl_vtable, registry_interface)
		},
		{
			"wl_display_create_queue",
			offsetof(struct wl_vtable, display_create_queue)
		},
		{
			"wl_display_roundtrip_queue",
			offsetof(struct wl_vtable, display_roundtrip_queue)
		},
        {
            "wl_display_dispatch_queue",
            offsetof(struct wl_vtable, display_dispatch_queue)
        },
		{
			"wl_event_queue_destroy",
			offsetof(struct wl_vtable, event_queue_destroy)
		},
		{
			"wl_proxy_create_wrapper",
			offsetof(struct wl_vtable, proxy_create_wrapper)
		},
		{
			"wl_proxy_wrapper_destroy",
			offsetof(struct wl_vtable, proxy_wrapper_destroy)
		},
		{
			"wl_proxy_create",
			offsetof(struct wl_vtable, proxy_create)
		},
		{
			"wl_proxy_destroy",
			offsetof(struct wl_vtable, proxy_destroy)
		},
		{
			"wl_proxy_marshal",
			offsetof(struct wl_vtable, proxy_marshal)
		},
		{
			"wl_proxy_add_listener",
			offsetof(struct wl_vtable, proxy_add_listener)
		},
		{
			"wl_proxy_set_queue",
			offsetof(struct wl_vtable, proxy_set_queue)
		},
		{ NULL, }
	};

	if (ctx->display_type != VA_DISPLAY_WAYLAND)
		return false;

    wl_linux_dmabuf_ctx->libwl_client_handle = dso_open("libwayland-client.so.0");
	if (!wl_linux_dmabuf_ctx->libwl_client_handle)
		goto end;

	dso_handle = wl_linux_dmabuf_ctx->libwl_client_handle;
	if (!dso_get_symbols(dso_handle, wl_vtable, sizeof(*wl_vtable),
						 libwl_client_symbols))
		goto end;

    drm_state = calloc(1, sizeof(*drm_state));
    if (!drm_state) {
        goto end;
    }

    drm_state->fd        = -1;
    drm_state->auth_type = 0;
    ctx->drm_state       = drm_state;

    vtable->has_prime_sharing = 0;

    /* Use wrapped wl_display with private event queue to prevent
     * thread safety issues with applications that e.g. run an event pump
     * parallel to libva initialization.
     * Using the default queue, events might get lost and crashes occur
     * because wl_display_roundtrip is not thread-safe with respect to the
     * same queue.
     */
    wl_linux_dmabuf_ctx->queue = wl_vtable->display_create_queue(ctx->native_dpy);;
    if (!wl_linux_dmabuf_ctx->queue) {
        goto end;
    }

    display = wl_vtable->proxy_create_wrapper(ctx->native_dpy);
    if (!display) {
        goto end;
    }
    
    wl_vtable->proxy_set_queue((struct wl_proxy *) display, wl_linux_dmabuf_ctx->queue);

    wl_linux_dmabuf_ctx->registry = display_get_registry(wl_vtable, display);
    if (!wl_linux_dmabuf_ctx->registry) {
        goto end;
    }

    wl_vtable->proxy_wrapper_destroy(display);
	registry_add_listener(wl_vtable, wl_linux_dmabuf_ctx->registry,
						  &registry_listener, ctx);

    if (wl_vtable->display_roundtrip_queue(ctx->native_dpy, wl_linux_dmabuf_ctx->queue) < 0) {
        goto end;
    }

    if (!wl_linux_dmabuf_ctx->has_linux_dmabuf)
        goto end;

    while (!wl_linux_dmabuf_ctx->default_feedback_done) {
        if (wl_vtable->display_dispatch_queue(ctx->native_dpy, wl_linux_dmabuf_ctx->queue) < 0) {
            goto end;
        }
    }

    if (drm_state->fd < 0)
        goto end;

    result = true;
    vtable->has_prime_sharing = true;
end:
    i965_wayland_linux_dmabuf_terminate(ctx);
    return result;
}

void
i965_wayland_linux_dmabuf_terminate(VADriverContextP ctx)
{
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct drm_state * const drm_state = ctx->drm_state;
    struct VADriverVTableWayland *vtable = ctx->vtable_wayland;
    struct intel_wayland_linux_dmabuf_context *dmabuf_output;

    if (ctx->display_type != VA_DISPLAY_WAYLAND)
		return;

	dmabuf_output = i965->dmabuf_output;
	if (!dmabuf_output)
		return;

    vtable->has_prime_sharing = 0;

    if (drm_state) {
        if (drm_state->fd >= 0) {
            close(drm_state->fd);
            drm_state->fd = -1;
        }
        free(ctx->drm_state);
        ctx->drm_state = NULL;
    }

    if (dmabuf_output->registry) {
		dmabuf_output->vtable.proxy_destroy((struct wl_proxy *)dmabuf_output->registry);
		dmabuf_output->registry = NULL;
	}

	if (dmabuf_output->queue) {
		dmabuf_output->vtable.event_queue_destroy(dmabuf_output->queue);
		dmabuf_output->queue = NULL;
	}

    if (dmabuf_output->libwl_client_handle) {
		dso_close(dmabuf_output->libwl_client_handle);
		dmabuf_output->libwl_client_handle = NULL;
	}

    free(i965->dmabuf_output);
	i965->dmabuf_output = NULL;
}