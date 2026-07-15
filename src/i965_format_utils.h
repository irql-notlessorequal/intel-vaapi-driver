/**
 * This file uses portions of code from the Intel Media Driver (iHD), which is licensed under the following:
 * 
 * Copyright (c) 2007-2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <drm_fourcc.h>

#include <va/va_backend.h>
#include "va_backend_compat.h"

// Locally define DRM_FORMAT values not available in older but still
// supported versions of libdrm.
#ifndef DRM_FORMAT_R8
#define DRM_FORMAT_R8        fourcc_code('R', '8', ' ', ' ')
#endif
#ifndef DRM_FORMAT_R16
#define DRM_FORMAT_R16       fourcc_code('R', '1', '6', ' ')
#endif
#ifndef DRM_FORMAT_GR88
#define DRM_FORMAT_GR88      fourcc_code('G', 'R', '8', '8')
#endif
#ifndef DRM_FORMAT_GR1616
#define DRM_FORMAT_GR1616    fourcc_code('G', 'R', '3', '2')
#endif
#ifndef DRM_FORMAT_XYUV8888
#define DRM_FORMAT_XYUV8888  fourcc_code('X', 'Y', 'U', 'V')
#endif
#ifndef DRM_FORMAT_Y210
#define DRM_FORMAT_Y210      fourcc_code('Y', '2', '1', '0')
#endif
#ifndef DRM_FORMAT_Y216
#define DRM_FORMAT_Y216      fourcc_code('Y', '2', '1', '6')
#endif
#ifndef DRM_FORMAT_Y410
#define DRM_FORMAT_Y410      fourcc_code('Y', '4', '1', '0')
#endif
#ifndef DRM_FORMAT_Y416
#define DRM_FORMAT_Y416      fourcc_code('Y', '4', '1', '6')
#endif
#ifndef DRM_FORMAT_P010
#define DRM_FORMAT_P010      fourcc_code('P', '0', '1', '0')
#endif
#ifndef DRM_FORMAT_P016
#define DRM_FORMAT_P016      fourcc_code('P', '0', '1', '6')
#endif

typedef struct
{
	uint32_t rt_format;
	uint32_t fourcc;
} RTFormatEntry;

typedef struct
{
	uint32_t va_fourcc;
	uint32_t drm_format;
} CompositeFmtEntry;

typedef struct
{
	uint32_t va_fourcc;
	uint32_t drm[3]; /* drm[0]=plane0, drm[1]=plane1, drm[2]=plane2 */
} PlaneFmtEntry;

void i965_GuessExpectedFourCC(uint32_t format, uint32_t *fourcc);

uint32_t drm_format_of_composite_object(uint32_t fourcc);

uint32_t drm_format_of_separate_plane(uint32_t fourcc, int plane);