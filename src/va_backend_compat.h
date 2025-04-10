/*
 * Copyright (C) 2012 Intel Corporation. All Rights Reserved.
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
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef VA_BACKEND_COMPAT_H
#define VA_BACKEND_COMPAT_H

#include <va/va_backend.h>

#if VA_CHECK_VERSION(0, 33, 0)
#include <va/va_drmcommon.h>

#define VA_CHECK_DRM_AUTH_TYPE(ctx, type) \
	(((struct drm_state *)(ctx)->drm_state)->auth_type == (type))

#else
#include <va/va_dricommon.h>
#define VA_CHECK_DRM_AUTH_TYPE(ctx, type) \
	(((struct dri_state *)(ctx)->dri_state)->driConnectedFlag == (type))

#define drm_state						dri_state
#define VA_DRM_AUTH_DRI1				VA_DRI1
#define VA_DRM_AUTH_DRI2				VA_DRI2
#define VA_DRM_AUTH_CUSTOM				VA_DUMMY
#endif

#if !VA_CHECK_VERSION(0, 35, 2)
#define VAProfileH264MultiviewHigh		15
#define VAProfileH264StereoHigh			16
#endif

#if !VA_CHECK_VERSION(0, 38, 1)
#define VA_RT_FORMAT_YUV420_10BPP		0x00000100
#define VA_FOURCC_P010					0x30313050
#define VA_FOURCC_P016					0x36313050
#endif

#if VA_CHECK_VERSION(1, 0, 0)
#define VAEncPackedHeaderMiscMask		0x80000000
#define VAEncPackedHeaderH264_SEI		(VAEncPackedHeaderMiscMask | 1)
#define VAEncPackedHeaderHEVC_SEI		(VAEncPackedHeaderMiscMask | 1)
#endif

#ifndef VA_FOURCC_ABGR
#define VA_FOURCC_ABGR			VA_FOURCC('A', 'B', 'G', 'R')
#endif

#ifndef VA_FOURCC_R5G6B5
#define VA_FOURCC_R5G6B5		VA_FOURCC('R', 'G', '1', '6')
#endif

#ifndef VA_FOURCC_R8G8B8
#define VA_FOURCC_R8G8B8		VA_FOURCC('R', 'G', '2', '4')
#endif

#ifndef VA_FOURCC_I420
#define VA_FOURCC_I420			VA_FOURCC('I', '4', '2', '0')
#endif

#if !VA_CHECK_VERSION(1, 8, 0)
#define VASurfaceAttribUsageHint				8
#define VA_SURFACE_ATTRIB_USAGE_HINT_GENERIC	0x00000000
#define VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER	0x00000002
#define VA_SURFACE_ATTRIB_USAGE_HINT_VPP_WRITE	0x00000008
#endif

#if !VA_CHECK_VERSION(1, 21, 0)
#define VA_MAPBUFFER_FLAG_DEFAULT	0
#define VA_MAPBUFFER_FLAG_READ		1
#define VA_MAPBUFFER_FLAG_WRITE		2
#endif

#endif /* VA_BACKEND_COMPAT_H */
