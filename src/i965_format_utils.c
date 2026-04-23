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
#include "i965_format_utils.h"

#define ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

static int format_entry_cmp(const void *key, const void *entry)
{
    uint32_t a = *(const uint32_t *)key;
    uint32_t b = *(const uint32_t *)entry;
    return (a > b) - (a < b);
}

static const RTFormatEntry rt_format_table[] = {
    { VA_RT_FORMAT_YUV420,       VA_FOURCC_NV12              },
    { VA_RT_FORMAT_YUV422,       VA_FOURCC_YUY2              },
    { VA_RT_FORMAT_YUV444,       VA_FOURCC_444P              },
    { VA_RT_FORMAT_YUV411,       VA_FOURCC_411P              },
    { VA_RT_FORMAT_YUV400,       VA_FOURCC('4','0','0','P')  },
    { VA_RT_FORMAT_YUV420_10BPP, VA_FOURCC_P010              },
    { VA_RT_FORMAT_YUV422_10,    VA_FOURCC_Y210              },
    { VA_RT_FORMAT_YUV444_10,    VA_FOURCC_Y410              },
    { VA_RT_FORMAT_YUV420_12,    VA_FOURCC_P012              },
#if VA_CHECK_VERSION(1, 9, 0)
    { VA_RT_FORMAT_YUV422_12,    VA_FOURCC_Y212              },
    { VA_RT_FORMAT_YUV444_12,    VA_FOURCC_Y412              },
#else
    { VA_RT_FORMAT_YUV422_12,    VA_FOURCC_Y216              },
    { VA_RT_FORMAT_YUV444_12,    VA_FOURCC_Y416              },
#endif
    { VA_RT_FORMAT_RGB16,        VA_FOURCC_R5G6B5            },
    /*
     * This should be VA_FOURCC_BGRA, however until Chromium
     * allows us to use VADRMPRIMESurfaceDescriptor, we're
     * stuck with this for now.
     */
    { VA_RT_FORMAT_RGB32,        VA_FOURCC_ARGB              },
    { VA_RT_FORMAT_RGBP,         VA_FOURCC_RGBP              },
#ifdef VA_RT_FORMAT_RGB32_10BPP
    { VA_RT_FORMAT_RGB32_10BPP,  VA_FOURCC_BGRA              },
#endif
};

void i965_GuessExpectedFourCC(uint32_t format, uint32_t *fourcc)
{
    const RTFormatEntry *e = bsearch(
        &format,
        rt_format_table,
        ARRAY_ELEMS(rt_format_table),
        sizeof(RTFormatEntry),
        format_entry_cmp);

    *fourcc = e ? e->fourcc : format;
}

static const CompositeFmtEntry composite_format_table[] = {
    {VA_FOURCC_Y800,        DRM_FORMAT_R8},
    {VA_FOURCC_P010,        DRM_FORMAT_P010},
    {VA_FOURCC_Y210,        DRM_FORMAT_Y210},
    {VA_FOURCC_Y410,        DRM_FORMAT_Y410},
    {VA_FOURCC_I420,        DRM_FORMAT_YUV420},
    {VA_FOURCC_A2B10G10R10, DRM_FORMAT_ABGR2101010},
    {VA_FOURCC_X2B10G10R10, DRM_FORMAT_XBGR2101010},
    {VA_FOURCC_A2R10G10B10, DRM_FORMAT_ARGB2101010},
    {VA_FOURCC_X2R10G10B10, DRM_FORMAT_XRGB2101010},
    {VA_FOURCC_P012,        DRM_FORMAT_P016},
#if VA_CHECK_VERSION(1, 9, 0)
    {VA_FOURCC_Y212,        DRM_FORMAT_Y216},
    {VA_FOURCC_Y412,        DRM_FORMAT_Y416},
#endif
    {VA_FOURCC_NV12,        DRM_FORMAT_NV12},
    {VA_FOURCC_YV12,        DRM_FORMAT_YVU420},
    {VA_FOURCC_YUY2,        DRM_FORMAT_YUYV},
    {VA_FOURCC_IMC3,        DRM_FORMAT_YUV420},
    {VA_FOURCC_P016,        DRM_FORMAT_P016},
    {VA_FOURCC_Y216,        DRM_FORMAT_Y216},
    {VA_FOURCC_Y416,        DRM_FORMAT_Y416},
    {VA_FOURCC_YV16,        DRM_FORMAT_YVU422},
    {VA_FOURCC_RGBA,        DRM_FORMAT_RGBA8888},
    {VA_FOURCC_BGRA,        DRM_FORMAT_BGRA8888},
    {VA_FOURCC_ARGB,        DRM_FORMAT_ARGB8888},
    {VA_FOURCC_XRGB,        DRM_FORMAT_XRGB8888},
    {VA_FOURCC_422H,        DRM_FORMAT_YUV422},
    {VA_FOURCC_444P,        DRM_FORMAT_YUV444},
    {VA_FOURCC_RGBP,        DRM_FORMAT_YUV444},
    {VA_FOURCC_BGRP,        DRM_FORMAT_YUV444},
    {VA_FOURCC_ABGR,        DRM_FORMAT_ABGR8888},
    {VA_FOURCC_XBGR,        DRM_FORMAT_XBGR8888},
    {VA_FOURCC_YVYU,        DRM_FORMAT_YVYU},
    {VA_FOURCC_422V,        DRM_FORMAT_YUV422},
    {VA_FOURCC_AYUV,        DRM_FORMAT_AYUV},
#if VA_CHECK_VERSION(1, 13, 0)
    {VA_FOURCC_XYUV,        DRM_FORMAT_XYUV8888},
#endif
    {VA_FOURCC_RGBX,        DRM_FORMAT_RGBX8888},
    {VA_FOURCC_BGRX,        DRM_FORMAT_BGRX8888},
    {VA_FOURCC_VYUY,        DRM_FORMAT_VYUY},
    {VA_FOURCC_UYVY,        DRM_FORMAT_UYVY},
};

uint32_t drm_format_of_composite_object(uint32_t fourcc)
{
    const CompositeFmtEntry *e = bsearch(
        &fourcc,
        composite_format_table,
        ARRAY_ELEMS(composite_format_table),
        sizeof(CompositeFmtEntry),
        format_entry_cmp);

    if (!e) {
        fprintf(stderr,
                "drm_format_of_composite_object: Unknown fourcc %#010x, "
                "returning zero.\n", fourcc);
        return 0;
    } else {
        return e->drm_format;
    }
}

static const PlaneFmtEntry plane_format_table[] = {
    {VA_FOURCC_Y800,        {DRM_FORMAT_R8, 0, 0}},
    {VA_FOURCC_I010,        {DRM_FORMAT_R16, DRM_FORMAT_R16, DRM_FORMAT_R16}},
    {VA_FOURCC_P010,        {DRM_FORMAT_R16, DRM_FORMAT_GR1616, 0}},
    {VA_FOURCC_Y210,        {DRM_FORMAT_Y210, 0, 0}},
    {VA_FOURCC_Y410,        {DRM_FORMAT_Y410, 0, 0}},
    {VA_FOURCC_I420,        {DRM_FORMAT_R8, DRM_FORMAT_R8, DRM_FORMAT_R8}},
    {VA_FOURCC_A2B10G10R10, {DRM_FORMAT_ABGR2101010, 0, 0}},
    {VA_FOURCC_X2B10G10R10, {DRM_FORMAT_XBGR2101010, 0, 0}},
    {VA_FOURCC_A2R10G10B10, {DRM_FORMAT_ARGB2101010, 0, 0}},
    {VA_FOURCC_X2R10G10B10, {DRM_FORMAT_XRGB2101010, 0, 0}},
    {VA_FOURCC_P012,        {DRM_FORMAT_R16, DRM_FORMAT_GR1616, 0}},
#if VA_CHECK_VERSION(1, 9, 0)
    {VA_FOURCC_Y212,        {DRM_FORMAT_Y216, 0, 0}},
    {VA_FOURCC_Y412,        {DRM_FORMAT_Y416, 0, 0}},
#endif
    {VA_FOURCC_NV12,        {DRM_FORMAT_R8, DRM_FORMAT_GR88, 0}},
    {VA_FOURCC_YV12,        {DRM_FORMAT_R8, DRM_FORMAT_R8, DRM_FORMAT_R8}},
    {VA_FOURCC_YUY2,        {DRM_FORMAT_YUYV, 0, 0}},
    {VA_FOURCC_IMC3,        {DRM_FORMAT_R8, DRM_FORMAT_R8, DRM_FORMAT_R8}},
    {VA_FOURCC_P016,        {DRM_FORMAT_R16, DRM_FORMAT_GR1616, 0}},
    {VA_FOURCC_Y216,        {DRM_FORMAT_Y216, 0, 0}},
    {VA_FOURCC_Y416,        {DRM_FORMAT_Y416, 0, 0}},
    {VA_FOURCC_YV16,        {DRM_FORMAT_R8, DRM_FORMAT_R8, DRM_FORMAT_R8}},
    {VA_FOURCC_RGBA,        {DRM_FORMAT_RGBA8888, 0, 0}},
    {VA_FOURCC_BGRA,        {DRM_FORMAT_BGRA8888, 0, 0}},
    {VA_FOURCC_ARGB,        {DRM_FORMAT_ARGB8888, 0, 0}},
    {VA_FOURCC_XRGB,        {DRM_FORMAT_XRGB8888, 0, 0}},
    {VA_FOURCC_422H,        {DRM_FORMAT_R8, DRM_FORMAT_R8, DRM_FORMAT_R8}},
    {VA_FOURCC_444P,        {DRM_FORMAT_R8, DRM_FORMAT_R8, DRM_FORMAT_R8}},
    {VA_FOURCC_RGBP,        {DRM_FORMAT_R8, DRM_FORMAT_R8, DRM_FORMAT_R8}},
    {VA_FOURCC_BGRP,        {DRM_FORMAT_R8, DRM_FORMAT_R8, DRM_FORMAT_R8}},
    {VA_FOURCC_ABGR,        {DRM_FORMAT_ABGR8888, 0, 0}},
    {VA_FOURCC_XBGR,        {DRM_FORMAT_XBGR8888, 0, 0}},
    {VA_FOURCC_YVYU,        {DRM_FORMAT_YVYU, 0, 0}},
    {VA_FOURCC_422V,        {DRM_FORMAT_R8, DRM_FORMAT_R8, DRM_FORMAT_R8}}, /* bug fix: plane2 */
    {VA_FOURCC_AYUV,        {DRM_FORMAT_AYUV, 0, 0}},
#if VA_CHECK_VERSION(1, 13, 0)
    {VA_FOURCC_XYUV,        {DRM_FORMAT_XYUV8888, 0, 0}},
#endif
    {VA_FOURCC_RGBX,        {DRM_FORMAT_RGBX8888, 0, 0}},
    {VA_FOURCC_BGRX,        {DRM_FORMAT_BGRX8888, 0, 0}},
    {VA_FOURCC_VYUY,        {DRM_FORMAT_VYUY, 0, 0}},
    {VA_FOURCC_UYVY,        {DRM_FORMAT_UYVY, 0, 0}},
};

uint32_t drm_format_of_separate_plane(uint32_t fourcc, int plane)
{
    if (plane < 0 || plane > 2) {
        fprintf(stderr,
                "drm_format_of_separate_plane: Invalid plane index %d for "
                "fourcc %#010x\n", plane, fourcc);
        return 0;
    }

    const PlaneFmtEntry *e = bsearch(
        &fourcc,
        plane_format_table,
        ARRAY_ELEMS(plane_format_table),
        sizeof(PlaneFmtEntry),
        format_entry_cmp);

    if (!e || e->drm[plane] == 0) {
        fprintf(stderr,
                "drm_format_of_separate_plane: Unknown fourcc %#010x or "
                "invalid plane %d, returning zero.\n", fourcc, plane);
        return 0;
    } else {
        return e->drm[plane];
    }
}