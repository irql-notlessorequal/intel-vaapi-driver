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

#ifndef I965_OUTPUT_X11_H
#define I965_OUTPUT_X11_H

#include <stdbool.h>
#include <va/va_backend.h>

bool
i965_output_x11_init(VADriverContextP ctx);

void
i965_output_x11_terminate(VADriverContextP ctx);

VAStatus
i965_put_surface_x11(
	VADriverContextP    ctx,
	VASurfaceID         surface,
	void               *draw,
	const VARectangle  *src_rect,
	const VARectangle  *dst_rect,
	const VARectangle  *cliprects,
	unsigned int        num_cliprects,
	unsigned int        flags
);

#endif /* I965_OUTPUT_X11_H */
