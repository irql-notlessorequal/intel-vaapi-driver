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
#include <stdio.h>

#include <drm_fourcc.h>

#include <va/va_backend.h>
#include "va_backend_compat.h"

#ifndef VA_FOURCC_ABGR
#define VA_FOURCC_ABGR          VA_FOURCC('A', 'B', 'G', 'R')
#endif

#ifndef VA_FOURCC_R5G6B5
#define VA_FOURCC_R5G6B5        VA_FOURCC('R','G','1', '6')
#endif

#ifndef VA_FOURCC_R8G8B8
#define VA_FOURCC_R8G8B8        VA_FOURCC('R','G','2', '4')
#endif

#ifndef VA_FOURCC_I420
#define VA_FOURCC_I420        VA_FOURCC('I','4','2', '0')
#endif

static inline int SubsampleForYUV422(int fourcc)
{
	switch (fourcc)
	{
		case VA_FOURCC_422V:
			return SUBSAMPLE_YUV422V;
		
		case VA_FOURCC_422H:
		case VA_FOURCC_YUY2:
		case VA_FOURCC_UYVY:
			return SUBSAMPLE_YUV422H;

		default: return SUBSAMPLE_YUV420;
	}
}

static inline int GetSubsamplingFromFormat(int format, int fourcc)
{
	switch (format)
	{
		case VA_RT_FORMAT_YUV420:
		case VA_RT_FORMAT_YUV420_10BPP:
			return SUBSAMPLE_YUV420;
		
		case VA_RT_FORMAT_YUV444:
			return SUBSAMPLE_YUV444;

		case VA_RT_FORMAT_YUV411:
			return SUBSAMPLE_YUV411;

		case VA_RT_FORMAT_YUV400:
			return SUBSAMPLE_YUV400;

		case VA_RT_FORMAT_YUV422:
			return SubsampleForYUV422(fourcc);

		case VA_RT_FORMAT_RGB32:
			return SUBSAMPLE_RGBX;

		default:
		{
			assert(!"Unknown subsampling for format: " + format);
			return -1;
		}
	}
}

static void i965_GuessExpectedFourCC(int format, int *fourcc)
{
	switch (format)
	{
	case VA_RT_FORMAT_YUV420:
		*fourcc = VA_FOURCC_NV12;
		break;
	case VA_RT_FORMAT_YUV420_12:
		*fourcc = VA_FOURCC_P012;
		break;
	case VA_RT_FORMAT_YUV422:
		*fourcc = VA_FOURCC_YUY2;
		break;
	case VA_RT_FORMAT_YUV422_10:
		*fourcc = VA_FOURCC_Y210;
		break;
	case VA_RT_FORMAT_YUV422_12:
#if VA_CHECK_VERSION(1, 9, 0)
		*fourcc = VA_FOURCC_Y212;
#else
		*fourcc = VA_FOURCC_Y216;
#endif
		break;
	case VA_RT_FORMAT_YUV444:
		*fourcc = VA_FOURCC_444P;
		break;
	case VA_RT_FORMAT_YUV444_10:
		*fourcc = VA_FOURCC_Y410;
		break;
	case VA_RT_FORMAT_YUV444_12:
#if VA_CHECK_VERSION(1, 9, 0)
		*fourcc = VA_FOURCC_Y412;
#else
		*fourcc = VA_FOURCC_Y416;
#endif
		break;
	case VA_RT_FORMAT_YUV411:
		*fourcc = VA_FOURCC_411P;
		break;
	case VA_RT_FORMAT_YUV400:
		*fourcc = VA_FOURCC('4', '0', '0', 'P');
		break;
	case VA_RT_FORMAT_YUV420_10BPP:
		*fourcc = VA_FOURCC_P010;
		break;
	case VA_RT_FORMAT_RGB16:
		*fourcc = VA_FOURCC_R5G6B5;
		break;
	/**
	 * We need to lie to Chromium and do ARGB instead of BGRA.
	 */
	case VA_RT_FORMAT_RGB32:
		*fourcc = VA_FOURCC_ARGB;
		break;
	case VA_RT_FORMAT_RGBP:
		*fourcc = VA_FOURCC_RGBP;
		break;
#ifdef VA_RT_FORMAT_RGB32_10BPP
	case VA_RT_FORMAT_RGB32_10BPP:
		*fourcc = VA_FOURCC_BGRA;
		break;
#endif
	case VA_FOURCC_NV12:
		*fourcc = VA_FOURCC_NV12;
		break;
	case VA_FOURCC_NV21:
		*fourcc = VA_FOURCC_NV21;
		break;
	case VA_FOURCC_ABGR:
		*fourcc = VA_FOURCC_ABGR;
		break;
	case VA_FOURCC_ARGB:
		*fourcc = VA_FOURCC_ARGB;
		break;
	case VA_FOURCC_XBGR:
		*fourcc = VA_FOURCC_XBGR;
		break;
	case VA_FOURCC_XRGB:
		*fourcc = VA_FOURCC_XRGB;
		break;
	case VA_FOURCC_R5G6B5:
		*fourcc = VA_FOURCC_R5G6B5;
		break;
	case VA_FOURCC_R8G8B8:
		*fourcc = VA_FOURCC_R8G8B8;
		break;
	case VA_FOURCC_YUY2:
		*fourcc = VA_FOURCC_YUY2;
		break;
	case VA_FOURCC_YV12:
		*fourcc = VA_FOURCC_YV12;
		break;
	case VA_FOURCC_422H:
		*fourcc = VA_FOURCC_422H;
		break;
	case VA_FOURCC_422V:
		*fourcc = VA_FOURCC_422V;
		break;
	case VA_FOURCC_P208:
		*fourcc = VA_FOURCC_P208;
		break;
	case VA_FOURCC_P010:
		*fourcc = VA_FOURCC_P010;
		break;
	case VA_FOURCC_P012:
		*fourcc = VA_FOURCC_P012;
		break;
	case VA_FOURCC_P016:
		*fourcc = VA_FOURCC_P016;
		break;
	case VA_FOURCC_Y210:
		*fourcc = VA_FOURCC_Y210;
		break;
#if VA_CHECK_VERSION(1, 9, 0)
	case VA_FOURCC_Y212:
		*fourcc = VA_FOURCC_Y212;
		break;
#endif
	case VA_FOURCC_Y216:
		*fourcc = VA_FOURCC_Y216;
		break;
	case VA_FOURCC_AYUV:
		*fourcc = VA_FOURCC_AYUV;
		break;
#if VA_CHECK_VERSION(1, 13, 0)
	case VA_FOURCC_XYUV:
		*fourcc = VA_FOURCC_XYUV;
		break;
#endif
	case VA_FOURCC_Y410:
		*fourcc = VA_FOURCC_Y410;
		break;
#if VA_CHECK_VERSION(1, 9, 0)
	case VA_FOURCC_Y412:
		*fourcc = VA_FOURCC_Y412;
		break;
#endif
	case VA_FOURCC_Y416:
		*fourcc = VA_FOURCC_Y416;
		break;
	case VA_FOURCC_I420:
		*fourcc = VA_FOURCC_I420;
		break;
	case VA_FOURCC_UYVY:
		*fourcc = VA_FOURCC_UYVY;
		break;
	default:
		return;
	}
}

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

static uint32_t drm_format_of_separate_plane(uint32_t fourcc, int plane)
{
	if (plane == 0)
	{
		switch (fourcc)
		{

		case VA_FOURCC_NV12:
		case VA_FOURCC_I420:
		case VA_FOURCC_IMC3:
		case VA_FOURCC_YV12:
		case VA_FOURCC_YV16:
		case VA_FOURCC_422H:
		case VA_FOURCC_422V:
		case VA_FOURCC_444P:
		case VA_FOURCC_Y800:
		case VA_FOURCC_RGBP:
		case VA_FOURCC_BGRP:
			return DRM_FORMAT_R8;
		case VA_FOURCC_P010:
		case VA_FOURCC_P012:
		case VA_FOURCC_P016:
		case VA_FOURCC_I010:
			return DRM_FORMAT_R16;

		case VA_FOURCC_YUY2:
			return DRM_FORMAT_YUYV;
		case VA_FOURCC_YVYU:
			return DRM_FORMAT_YVYU;
		case VA_FOURCC_VYUY:
			return DRM_FORMAT_VYUY;
		case VA_FOURCC_UYVY:
			return DRM_FORMAT_UYVY;
		case VA_FOURCC_AYUV:
			return DRM_FORMAT_AYUV;
#if VA_CHECK_VERSION(1, 13, 0)
		case VA_FOURCC_XYUV:
			return DRM_FORMAT_XYUV8888;
#endif
		case VA_FOURCC_Y210:
			return DRM_FORMAT_Y210;
		case VA_FOURCC_Y216:
			return DRM_FORMAT_Y216;
		case VA_FOURCC_Y410:
			return DRM_FORMAT_Y410;
		case VA_FOURCC_Y416:
			return DRM_FORMAT_Y416;
#if VA_CHECK_VERSION(1, 9, 0)
		case VA_FOURCC_Y212:
			return DRM_FORMAT_Y216;
		case VA_FOURCC_Y412:
			return DRM_FORMAT_Y416;
#endif

		case VA_FOURCC_ARGB:
			return DRM_FORMAT_ARGB8888;
		case VA_FOURCC_ABGR:
			return DRM_FORMAT_ABGR8888;
		case VA_FOURCC_RGBA:
			return DRM_FORMAT_RGBA8888;
		case VA_FOURCC_BGRA:
			return DRM_FORMAT_BGRA8888;
		case VA_FOURCC_XRGB:
			return DRM_FORMAT_XRGB8888;
		case VA_FOURCC_XBGR:
			return DRM_FORMAT_XBGR8888;
		case VA_FOURCC_RGBX:
			return DRM_FORMAT_RGBX8888;
		case VA_FOURCC_BGRX:
			return DRM_FORMAT_BGRX8888;
		case VA_FOURCC_A2R10G10B10:
			return DRM_FORMAT_ARGB2101010;
		case VA_FOURCC_A2B10G10R10:
			return DRM_FORMAT_ABGR2101010;
		case VA_FOURCC_X2R10G10B10:
			return DRM_FORMAT_XRGB2101010;
		case VA_FOURCC_X2B10G10R10:
			return DRM_FORMAT_XBGR2101010;

		default:
			{
				fprintf(stderr, "drm_format_of_separate_plane: Unknown fourcc %#010x, returning zero. (plane=%u)\r\n", fourcc, plane);
				return 0;
			}
		}
	}
	else
	{
		switch (fourcc)
		{

		case VA_FOURCC_NV12:
			return DRM_FORMAT_GR88;
		case VA_FOURCC_I420:
		case VA_FOURCC_IMC3:
		case VA_FOURCC_YV12:
		case VA_FOURCC_YV16:
		case VA_FOURCC_422H:
		case VA_FOURCC_422V:
		case VA_FOURCC_444P:
		case VA_FOURCC_RGBP:
		case VA_FOURCC_BGRP:
			return DRM_FORMAT_R8;
		case VA_FOURCC_P010:
		case VA_FOURCC_P012:
		case VA_FOURCC_P016:
			return DRM_FORMAT_GR1616;
		case VA_FOURCC_I010:
			return DRM_FORMAT_R16;

		default:
			{
				fprintf(stderr, "drm_format_of_separate_plane: Unknown fourcc %#010x, returning zero. (plane=%u)\r\n", fourcc, plane);
				return 0;
			}
		}
	}
}

static uint32_t drm_format_of_composite_object(uint32_t fourcc)
{
	switch (fourcc)
	{

	case VA_FOURCC_NV12:
		return DRM_FORMAT_NV12;
	case VA_FOURCC_I420:
		return DRM_FORMAT_YUV420;
	case VA_FOURCC_IMC3:
		return DRM_FORMAT_YUV420;
	case VA_FOURCC_YV12:
		return DRM_FORMAT_YVU420;
	case VA_FOURCC_YV16:
		return DRM_FORMAT_YVU422;
	case VA_FOURCC_422H:
		return DRM_FORMAT_YUV422;
	case VA_FOURCC_422V:
		return DRM_FORMAT_YUV422;
	case VA_FOURCC_444P:
		return DRM_FORMAT_YUV444;
	case VA_FOURCC_YUY2:
		return DRM_FORMAT_YUYV;
	case VA_FOURCC_YVYU:
		return DRM_FORMAT_YVYU;
	case VA_FOURCC_VYUY:
		return DRM_FORMAT_VYUY;
	case VA_FOURCC_UYVY:
		return DRM_FORMAT_UYVY;
	case VA_FOURCC_AYUV:
		return DRM_FORMAT_AYUV;
#if VA_CHECK_VERSION(1, 13, 0)
	case VA_FOURCC_XYUV:
		return DRM_FORMAT_XYUV8888;
#endif
	case VA_FOURCC_Y210:
		return DRM_FORMAT_Y210;
#if VA_CHECK_VERSION(1, 9, 0)
	case VA_FOURCC_Y212:
		return DRM_FORMAT_Y216;
#endif
	case VA_FOURCC_Y216:
		return DRM_FORMAT_Y216;
	case VA_FOURCC_Y410:
		return DRM_FORMAT_Y410;
#if VA_CHECK_VERSION(1, 9, 0)
	case VA_FOURCC_Y412:
		return DRM_FORMAT_Y416;
#endif
	case VA_FOURCC_Y416:
		return DRM_FORMAT_Y416;
	case VA_FOURCC_Y800:
		return DRM_FORMAT_R8;
	case VA_FOURCC_P010:
		return DRM_FORMAT_P010;
	case VA_FOURCC_P012:
		return DRM_FORMAT_P016;
	case VA_FOURCC_P016:
		return DRM_FORMAT_P016;
	case VA_FOURCC_ARGB:
		return DRM_FORMAT_ARGB8888;
	case VA_FOURCC_ABGR:
		return DRM_FORMAT_ABGR8888;
	case VA_FOURCC_RGBA:
		return DRM_FORMAT_RGBA8888;
	case VA_FOURCC_BGRA:
		return DRM_FORMAT_BGRA8888;
	case VA_FOURCC_XRGB:
		return DRM_FORMAT_XRGB8888;
	case VA_FOURCC_XBGR:
		return DRM_FORMAT_XBGR8888;
	case VA_FOURCC_RGBX:
		return DRM_FORMAT_RGBX8888;
	case VA_FOURCC_BGRX:
		return DRM_FORMAT_BGRX8888;
	case VA_FOURCC_A2R10G10B10:
		return DRM_FORMAT_ARGB2101010;
	case VA_FOURCC_A2B10G10R10:
		return DRM_FORMAT_ABGR2101010;
	case VA_FOURCC_X2R10G10B10:
		return DRM_FORMAT_XRGB2101010;
	case VA_FOURCC_X2B10G10R10:
		return DRM_FORMAT_XBGR2101010;

	default:
		{
			fprintf(stderr, "drm_format_of_composite_object: Unknown fourcc %#010x, returning zero.\r\n", fourcc);
			return 0;
		}
	}
}