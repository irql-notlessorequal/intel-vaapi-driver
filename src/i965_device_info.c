/*
 * Copyright (C) 2014 Intel Corporation
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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "i965_drv_video.h"

#include <string.h>
#include <strings.h>
#include <errno.h>
#include <cpuid.h>

/* Extra set of chroma formats supported for H.264 decoding (beyond YUV 4:2:0) */
#define EXTRA_H264_DEC_CHROMA_FORMATS \
	(VA_RT_FORMAT_YUV400)

/* Extra set of chroma formats supported for JPEG decoding (beyond YUV 4:2:0) */
#define EXTRA_JPEG_DEC_CHROMA_FORMATS \
	(VA_RT_FORMAT_YUV400 | VA_RT_FORMAT_YUV411 | VA_RT_FORMAT_YUV422 | \
	 VA_RT_FORMAT_YUV444)

/* Extra set of chroma formats supported for JPEG encoding (beyond YUV 4:2:0) */
#define EXTRA_JPEG_ENC_CHROMA_FORMATS \
	(VA_RT_FORMAT_YUV400| VA_RT_FORMAT_YUV422 | VA_RT_FORMAT_YUV444 | VA_RT_FORMAT_RGB32)

#define EXTRA_HEVC_DEC_CHROMA_FORMATS \
	(VA_RT_FORMAT_YUV420_10BPP)

#define EXTRA_VP9_DEC_CHROMA_FORMATS \
	(VA_RT_FORMAT_YUV420_10BPP)

/* Defines VA profile as a 32-bit unsigned integer mask */
#define VA_PROFILE_MASK(PROFILE) \
	(1U << VAProfile##PROFILE)

#define VP9_PROFILE_MASK(PROFILE) \
	(1U << PROFILE)

extern struct hw_context *i965_proc_context_init(VADriverContextP, struct object_config *);
extern struct hw_context *g4x_dec_hw_context_init(VADriverContextP, struct object_config *);

extern void g4x_get_hw_formats(VADriverContextP ctx, struct object_config *obj_config,
	struct i965_driver_data* data, int *i, VASurfaceAttrib *attribs);

extern bool genx_render_init(VADriverContextP);

static struct hw_codec_info g4x_hw_codec_info = {
	.dec_hw_context_init = g4x_dec_hw_context_init,
	.enc_hw_context_init = NULL,
	.proc_hw_context_init = NULL,
	.render_init = genx_render_init,
	.post_processing_context_init = NULL,
	.get_hw_formats = g4x_get_hw_formats,

	.max_width = 2048,
	.max_height = 2048,

	.max_width_mpeg2 = 1920,
	.max_height_mpeg2 = 1088,

	/* Unsupported in HW */
	.max_width_jpeg = -1,
	.max_height_jpeg = -1,

	.min_linear_wpitch = 16,
	.min_linear_hpitch = 4,

	.has_mpeg2_decoding = 1,

	.num_filters = 0,
};

extern struct hw_context *ironlake_dec_hw_context_init(VADriverContextP, struct object_config *);
extern void i965_post_processing_context_init(VADriverContextP, void *, struct intel_batchbuffer *);

extern void ironlake_get_hw_formats(VADriverContextP ctx, struct object_config *obj_config,
	struct i965_driver_data* data, int *i, VASurfaceAttrib *attribs);

static struct hw_codec_info ilk_hw_codec_info = {
	.dec_hw_context_init = ironlake_dec_hw_context_init,
	.enc_hw_context_init = NULL,
	.proc_hw_context_init = i965_proc_context_init,
	.render_init = genx_render_init,
	.post_processing_context_init = i965_post_processing_context_init,
	.get_hw_formats = ironlake_get_hw_formats,

	.max_width = 2048,
	.max_height = 2048,

	.max_width_mpeg2 = 1920,
	.max_height_mpeg2 = 1088,

	/* Unsupported in HW */
	.max_width_jpeg = -1,
	.max_height_jpeg = -1,

	.min_linear_wpitch = 16,
	.min_linear_hpitch = 4,

	.has_mpeg2_decoding = 1,
	.has_h264_decoding = 1,
	.has_vpp = 1,
	.has_accelerated_putimage = 1,

	.num_filters = 0,
};

static void gen6_hw_codec_preinit(VADriverContextP ctx, struct hw_codec_info *codec_info);

extern void gen6_get_hw_formats(VADriverContextP ctx, struct object_config *obj_config,
	struct i965_driver_data* data, int *i, VASurfaceAttrib *attribs);
extern struct hw_context *gen6_dec_hw_context_init(VADriverContextP, struct object_config *);
extern struct hw_context *gen6_enc_hw_context_init(VADriverContextP, struct object_config *);

static struct hw_codec_info snb_hw_codec_info = {
	.dec_hw_context_init = gen6_dec_hw_context_init,
	.enc_hw_context_init = gen6_enc_hw_context_init,
	.proc_hw_context_init = i965_proc_context_init,
	.render_init = genx_render_init,
	.post_processing_context_init = i965_post_processing_context_init,
	.preinit_hw_codec = gen6_hw_codec_preinit,
	.get_hw_formats = gen6_get_hw_formats,

	.max_width = 2048,
	.max_height = 2048,

	.max_width_mpeg2 = 1920,
	.max_height_mpeg2 = 1088,

	/* Unsupported in HW */
	.max_width_jpeg = -1,
	.max_height_jpeg = -1,

	.min_linear_wpitch = 16,
	.min_linear_hpitch = 4,

	.h264_mvc_dec_profiles = VA_PROFILE_MASK(H264StereoHigh),
	.h264_dec_chroma_formats = EXTRA_H264_DEC_CHROMA_FORMATS,

	.has_mpeg2_decoding = 1,
	.has_h264_decoding = 1,
	.has_h264_encoding = 1,
	.has_vc1_decoding = 1,
	.has_vpp = 1,
	.has_accelerated_getimage = 1,
	.has_accelerated_putimage = 1,
	.has_tiled_surface = 1,
	.has_di_motion_adptive = 1,

	.h264_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR,

	.num_filters = 2,
	.filters = {
		{VAProcFilterNoiseReduction, I965_RING_NULL},
		{VAProcFilterDeinterlacing, I965_RING_NULL},
	},
};

static void gen7_hw_codec_preinit(VADriverContextP ctx, struct hw_codec_info *codec_info);

extern void gen7_get_hw_formats(VADriverContextP ctx, struct object_config *obj_config,
	struct i965_driver_data* data, int *i, VASurfaceAttrib *attribs);
extern struct hw_context *gen7_dec_hw_context_init(VADriverContextP, struct object_config *);
extern struct hw_context *gen7_enc_hw_context_init(VADriverContextP, struct object_config *);

static struct hw_codec_info ivb_hw_codec_info = {
	.dec_hw_context_init = gen7_dec_hw_context_init,
	.enc_hw_context_init = gen7_enc_hw_context_init,
	.proc_hw_context_init = i965_proc_context_init,
	.render_init = genx_render_init,
	.post_processing_context_init = i965_post_processing_context_init,
	.preinit_hw_codec = gen7_hw_codec_preinit,
	.get_hw_formats = gen7_get_hw_formats,

	.max_width = 4096,
	.max_height = 4096,

	.max_width_mpeg2 = 1920,
	.max_height_mpeg2 = 1088,

	.max_width_jpeg = 16384,
	.max_height_jpeg = 16384,

	.min_linear_wpitch = 64,
	.min_linear_hpitch = 4,

	.h264_mvc_dec_profiles = VA_PROFILE_MASK(H264StereoHigh),
	.h264_dec_chroma_formats = EXTRA_H264_DEC_CHROMA_FORMATS,
	.jpeg_dec_chroma_formats = EXTRA_JPEG_DEC_CHROMA_FORMATS,

	.has_mpeg2_decoding = 1,
	.has_mpeg2_encoding = 1,
	.has_h264_decoding = 1,
	.has_h264_encoding = 1,
	.has_vc1_decoding = 1,
	.has_jpeg_decoding = 1,
	.has_vpp = 1,
	.has_accelerated_getimage = 1,
	.has_accelerated_putimage = 1,
	.has_tiled_surface = 1,
	.has_di_motion_adptive = 1,
	.has_di_motion_compensated = 1,

	.supports_short_slice_dec = 1,

	.h264_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR,

	.num_filters = 2,
	.filters = {
		{VAProcFilterNoiseReduction, I965_RING_NULL},
		{VAProcFilterDeinterlacing, I965_RING_NULL},
	},
};

static void hsw_hw_codec_preinit(VADriverContextP ctx, struct hw_codec_info *codec_info);

extern struct hw_context *gen75_dec_hw_context_init(VADriverContextP, struct object_config *);
extern struct hw_context *gen75_enc_hw_context_init(VADriverContextP, struct object_config *);
extern struct hw_context *gen75_proc_context_init(VADriverContextP, struct object_config *);
static struct hw_codec_info hsw_hw_codec_info = {
	.dec_hw_context_init = gen75_dec_hw_context_init,
	.enc_hw_context_init = gen75_enc_hw_context_init,
	.proc_hw_context_init = gen75_proc_context_init,
	.render_init = genx_render_init,
	.post_processing_context_init = i965_post_processing_context_init,
	.preinit_hw_codec = hsw_hw_codec_preinit,
	.get_hw_formats = gen7_get_hw_formats,

	.max_width = 4096,
	.max_height = 4096,

	.max_width_mpeg2 = 1920,
	.max_height_mpeg2 = 1088,

	.max_width_jpeg = 16384,
	.max_height_jpeg = 16384,

	.min_linear_wpitch = 64,
	.min_linear_hpitch = 4,

	.h264_mvc_dec_profiles = (VA_PROFILE_MASK(H264StereoHigh) |
							  VA_PROFILE_MASK(H264MultiviewHigh)),
	.h264_dec_chroma_formats = EXTRA_H264_DEC_CHROMA_FORMATS,
	.jpeg_dec_chroma_formats = EXTRA_JPEG_DEC_CHROMA_FORMATS,

	.has_mpeg2_decoding = 1,
	.has_mpeg2_encoding = 1,
	.has_h264_decoding = 1,
	.has_h264_encoding = 1,
	.has_vc1_decoding = 1,
	.has_jpeg_decoding = 1,
	.has_vpp = 1,
	.has_accelerated_getimage = 1,
	.has_accelerated_putimage = 1,
	.has_tiled_surface = 1,
	.has_di_motion_adptive = 1,
	.has_di_motion_compensated = 1,
	.has_h264_mvc_encoding = 1,

	.supports_short_slice_dec = 1,

	.h264_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR,

	.num_filters = 5,
	.filters = {
		{VAProcFilterNoiseReduction, I965_RING_VEBOX},
		{VAProcFilterDeinterlacing, I965_RING_VEBOX},
		{VAProcFilterSharpening, I965_RING_NULL},
		{VAProcFilterColorBalance, I965_RING_VEBOX},
		{VAProcFilterSkinToneEnhancement, I965_RING_VEBOX},
	},
};

extern struct hw_context *gen8_dec_hw_context_init(VADriverContextP, struct object_config *);
extern struct hw_context *gen8_enc_hw_context_init(VADriverContextP, struct object_config *);
extern void gen8_post_processing_context_init(VADriverContextP, void *, struct intel_batchbuffer *);

extern void gen8_get_hw_formats(VADriverContextP ctx, struct object_config *obj_config,
	struct i965_driver_data* data, int *i, VASurfaceAttrib *attribs);

static struct hw_codec_info bdw_hw_codec_info = {
	.dec_hw_context_init = gen8_dec_hw_context_init,
	.enc_hw_context_init = gen8_enc_hw_context_init,
	.proc_hw_context_init = gen75_proc_context_init,
	.render_init = gen8_render_init,
	.post_processing_context_init = gen8_post_processing_context_init,
	.get_hw_formats = gen8_get_hw_formats,

	.max_width = 4096,
	.max_height = 4096,

	.max_width_mpeg2 = 1920,
	.max_height_mpeg2 = 1088,

	.max_width_jpeg = 16384,
	.max_height_jpeg = 16384,

	.min_linear_wpitch = 64,
	.min_linear_hpitch = 4,

	.h264_mvc_dec_profiles = (VA_PROFILE_MASK(H264StereoHigh) |
							  VA_PROFILE_MASK(H264MultiviewHigh)),
	.h264_dec_chroma_formats = EXTRA_H264_DEC_CHROMA_FORMATS,
	.jpeg_dec_chroma_formats = EXTRA_JPEG_DEC_CHROMA_FORMATS,

	.has_mpeg2_decoding = 1,
	.has_mpeg2_encoding = 1,
	.has_h264_decoding = 1,
	.has_h264_encoding = 1,
	.has_vc1_decoding = 1,
	.has_jpeg_decoding = 1,
	.has_vpp = 1,
	.has_accelerated_getimage = 1,
	.has_accelerated_putimage = 1,
	.has_tiled_surface = 1,
	.has_di_motion_adptive = 1,
	.has_di_motion_compensated = 1,
	.has_vp8_decoding = 1,
	.has_h264_mvc_encoding = 1,

	.supports_short_slice_dec = 1,

	.h264_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR,

	.num_filters = 5,
	.filters = {
		{VAProcFilterNoiseReduction, I965_RING_VEBOX},
		{VAProcFilterDeinterlacing, I965_RING_VEBOX},
		{VAProcFilterSharpening, I965_RING_NULL}, /* need to rebuild the shader for BDW */
		{VAProcFilterColorBalance, I965_RING_VEBOX},
		{VAProcFilterSkinToneEnhancement, I965_RING_VEBOX},
	},
};

extern struct hw_context *gen9_dec_hw_context_init(VADriverContextP, struct object_config *);

static struct hw_codec_info chv_hw_codec_info = {
	.dec_hw_context_init = gen9_dec_hw_context_init,
	.enc_hw_context_init = gen8_enc_hw_context_init,
	.proc_hw_context_init = gen75_proc_context_init,
	.render_init = gen8_render_init,
	.post_processing_context_init = gen8_post_processing_context_init,
	.get_hw_formats = gen8_get_hw_formats,

	.max_width = 4096,
	.max_height = 4096,

	.max_width_mpeg2 = 1920,
	.max_height_mpeg2 = 1088,

	.max_width_jpeg = 16384,
	.max_height_jpeg = 16384,

	.min_linear_wpitch = 64,
	.min_linear_hpitch = 4,

	.h264_mvc_dec_profiles = (VA_PROFILE_MASK(H264StereoHigh) |
							  VA_PROFILE_MASK(H264MultiviewHigh)),
	.h264_dec_chroma_formats = EXTRA_H264_DEC_CHROMA_FORMATS,
	.jpeg_dec_chroma_formats = EXTRA_JPEG_DEC_CHROMA_FORMATS,
	.jpeg_enc_chroma_formats = EXTRA_JPEG_ENC_CHROMA_FORMATS,

	.has_mpeg2_decoding = 1,
	.has_mpeg2_encoding = 1,
	.has_h264_decoding = 1,
	.has_h264_encoding = 1,
	.has_vc1_decoding = 1,
	.has_jpeg_decoding = 1,
	.has_jpeg_encoding = 1,
	.has_vpp = 1,
	.has_accelerated_getimage = 1,
	.has_accelerated_putimage = 1,
	.has_tiled_surface = 1,
	.has_di_motion_adptive = 1,
	.has_di_motion_compensated = 1,
	.has_vp8_decoding = 1,
	.has_vp8_encoding = 1,
	.has_h264_mvc_encoding = 1,
	.has_hevc_decoding = 1,

	.supports_short_slice_dec = 1,

	.h264_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR,

	.num_filters = 5,
	.filters = {
		{VAProcFilterNoiseReduction, I965_RING_VEBOX},
		{VAProcFilterDeinterlacing, I965_RING_VEBOX},
		{VAProcFilterSharpening, I965_RING_NULL}, /* need to rebuild the shader for BDW */
		{VAProcFilterColorBalance, I965_RING_VEBOX},
		{VAProcFilterSkinToneEnhancement, I965_RING_VEBOX},
	},
};

static void gen9_hw_codec_preinit(VADriverContextP ctx, struct hw_codec_info *codec_info);

extern struct hw_context *gen9_enc_hw_context_init(VADriverContextP, struct object_config *);
extern void gen9_post_processing_context_init(VADriverContextP, void *, struct intel_batchbuffer *);
extern void gen9_max_resolution(struct i965_driver_data *, struct object_config *, int *, int *);
static struct hw_codec_info skl_hw_codec_info = {
	.dec_hw_context_init = gen9_dec_hw_context_init,
	.enc_hw_context_init = gen9_enc_hw_context_init,
	.proc_hw_context_init = gen75_proc_context_init,
	.render_init = gen9_render_init,
	.post_processing_context_init = gen9_post_processing_context_init,
	.max_resolution = gen9_max_resolution,
	.preinit_hw_codec = gen9_hw_codec_preinit,
	.get_hw_formats = gen8_get_hw_formats,

	.max_width = 4096,	/* default. See max_resolution */
	.max_height = 4096, /* default. See max_resolution */

	.max_width_mpeg2 = 1920,
	.max_height_mpeg2 = 1088,

	.max_width_jpeg = 16384,
	.max_height_jpeg = 16384,

	.min_linear_wpitch = 64,
	.min_linear_hpitch = 4,

	.h264_mvc_dec_profiles = (VA_PROFILE_MASK(H264StereoHigh) |
							  VA_PROFILE_MASK(H264MultiviewHigh)),
	.h264_dec_chroma_formats = EXTRA_H264_DEC_CHROMA_FORMATS,
	.jpeg_dec_chroma_formats = EXTRA_JPEG_DEC_CHROMA_FORMATS,
	.jpeg_enc_chroma_formats = EXTRA_JPEG_ENC_CHROMA_FORMATS,

	.has_mpeg2_decoding = 1,
	.has_mpeg2_encoding = 1,
	.has_h264_decoding = 1,
	.has_h264_encoding = 1,
	.has_vc1_decoding = 1,
	.has_jpeg_decoding = 1,
	.has_jpeg_encoding = 1,
	.has_vpp = 1,
	.has_accelerated_getimage = 1,
	.has_accelerated_putimage = 1,
	.has_tiled_surface = 1,
	.has_di_motion_adptive = 1,
	.has_di_motion_compensated = 1,
	.has_vp8_decoding = 1,
	.has_vp8_encoding = 1,
	.has_h264_mvc_encoding = 1,
	.has_hevc_decoding = 1,
	.has_hevc_encoding = 1,
	.has_lp_h264_encoding = 1,
	.has_fei_h264_encoding = 1,
	.has_h264_preenc = 1,

	.supports_short_slice_dec = 1,

	.lp_h264_brc_mode = VA_RC_CQP,
	.h264_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR | VA_RC_MB,

	.num_filters = 5,
	.filters = {
		{VAProcFilterNoiseReduction, I965_RING_VEBOX},
		{VAProcFilterDeinterlacing, I965_RING_VEBOX},
		{VAProcFilterSharpening, I965_RING_NULL}, /* need to rebuild the shader for BDW */
		{VAProcFilterColorBalance, I965_RING_VEBOX},
		{VAProcFilterSkinToneEnhancement, I965_RING_VEBOX},
	},
};

static struct hw_codec_info bxt_hw_codec_info = {
	.dec_hw_context_init = gen9_dec_hw_context_init,
	.enc_hw_context_init = gen9_enc_hw_context_init,
	.proc_hw_context_init = gen75_proc_context_init,
	.render_init = gen9_render_init,
	.post_processing_context_init = gen9_post_processing_context_init,
	.max_resolution = gen9_max_resolution,
	.preinit_hw_codec = gen9_hw_codec_preinit,
	.get_hw_formats = gen8_get_hw_formats,

	.max_width = 4096,	/* default. See max_resolution */
	.max_height = 4096, /* default. See max_resolution */

	.max_width_mpeg2 = 1920,
	.max_height_mpeg2 = 1088,

	.max_width_jpeg = 16384,
	.max_height_jpeg = 16384,

	.min_linear_wpitch = 64,
	.min_linear_hpitch = 4,

	.h264_mvc_dec_profiles = (VA_PROFILE_MASK(H264StereoHigh) |
							  VA_PROFILE_MASK(H264MultiviewHigh)),
	.vp9_dec_profiles = VP9_PROFILE_MASK(0),

	.h264_dec_chroma_formats = EXTRA_H264_DEC_CHROMA_FORMATS,
	.jpeg_dec_chroma_formats = EXTRA_JPEG_DEC_CHROMA_FORMATS,
	.jpeg_enc_chroma_formats = EXTRA_JPEG_ENC_CHROMA_FORMATS,
	.hevc_dec_chroma_formats = EXTRA_HEVC_DEC_CHROMA_FORMATS,

	.has_mpeg2_decoding = 1,
	.has_h264_decoding = 1,
	.has_h264_encoding = 1,
	.has_vc1_decoding = 1,
	.has_jpeg_decoding = 1,
	.has_jpeg_encoding = 1,
	.has_vpp = 1,
	.has_accelerated_getimage = 1,
	.has_accelerated_putimage = 1,
	.has_tiled_surface = 1,
	.has_di_motion_adptive = 1,
	.has_di_motion_compensated = 1,
	.has_vp8_decoding = 1,
	.has_vp8_encoding = 1,
	.has_h264_mvc_encoding = 1,
	.has_hevc_decoding = 1,
	.has_hevc_encoding = 1,
	.has_hevc10_decoding = 1,
	.has_vp9_decoding = 1,
	.has_vpp_p010 = 1,
	.has_lp_h264_encoding = 1,

	.supports_short_slice_dec = 1,

	.lp_h264_brc_mode = VA_RC_CQP,
	.h264_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR | VA_RC_MB,

	.num_filters = 5,
	.filters = {
		{VAProcFilterNoiseReduction, I965_RING_VEBOX},
		{VAProcFilterDeinterlacing, I965_RING_VEBOX},
		{VAProcFilterSharpening, I965_RING_NULL},
		{VAProcFilterColorBalance, I965_RING_VEBOX},
		{VAProcFilterSkinToneEnhancement, I965_RING_VEBOX},
	},
};

static struct hw_codec_info kbl_hw_codec_info = {
	.dec_hw_context_init = gen9_dec_hw_context_init,
	.enc_hw_context_init = gen9_enc_hw_context_init,
	.proc_hw_context_init = gen75_proc_context_init,
	.render_init = gen9_render_init,
	.post_processing_context_init = gen9_post_processing_context_init,
	.max_resolution = gen9_max_resolution,
	.preinit_hw_codec = gen9_hw_codec_preinit,
	.get_hw_formats = gen8_get_hw_formats,

	.max_width = 4096,	/* default. See max_resolution */
	.max_height = 4096, /* default. See max_resolution */

	.max_width_mpeg2 = 1920,
	.max_height_mpeg2 = 1088,

	.max_width_jpeg = 16384,
	.max_height_jpeg = 16384,

	.min_linear_wpitch = 64,
	.min_linear_hpitch = 4,

	.h264_mvc_dec_profiles = (VA_PROFILE_MASK(H264StereoHigh) |
							  VA_PROFILE_MASK(H264MultiviewHigh)),
	.vp9_dec_profiles = VP9_PROFILE_MASK(0) |
						VP9_PROFILE_MASK(2),
	.vp9_enc_profiles = VP9_PROFILE_MASK(0),

	.h264_dec_chroma_formats = EXTRA_H264_DEC_CHROMA_FORMATS,
	.jpeg_dec_chroma_formats = EXTRA_JPEG_DEC_CHROMA_FORMATS,
	.jpeg_enc_chroma_formats = EXTRA_JPEG_ENC_CHROMA_FORMATS,
	.hevc_dec_chroma_formats = EXTRA_HEVC_DEC_CHROMA_FORMATS,
	.vp9_dec_chroma_formats = EXTRA_VP9_DEC_CHROMA_FORMATS,

	.has_mpeg2_decoding = 1,
	.has_mpeg2_encoding = 1,
	.has_h264_decoding = 1,
	.has_h264_encoding = 1,
	.has_vc1_decoding = 1,
	.has_jpeg_decoding = 1,
	.has_jpeg_encoding = 1,
	.has_vpp = 1,
	.has_accelerated_getimage = 1,
	.has_accelerated_putimage = 1,
	.has_tiled_surface = 1,
	.has_di_motion_adptive = 1,
	.has_di_motion_compensated = 1,
	.has_vp8_decoding = 1,
	.has_vp8_encoding = 1,
	.has_h264_mvc_encoding = 1,
	.has_hevc_decoding = 1,
	.has_hevc_encoding = 1,
	.has_hevc10_encoding = 1,
	.has_hevc10_decoding = 1,
	.has_vp9_decoding = 1,
	.has_vpp_p010 = 1,
	.has_vp9_encoding = 1,
	.has_lp_h264_encoding = 1,

	.supports_short_slice_dec = 1,

	.lp_h264_brc_mode = VA_RC_CQP,
	.h264_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR | VA_RC_MB,

	.vp9_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR,

	.num_filters = 5,
	.filters = {
		{VAProcFilterNoiseReduction, I965_RING_VEBOX},
		{VAProcFilterDeinterlacing, I965_RING_VEBOX},
		{VAProcFilterSharpening, I965_RING_NULL},
		{VAProcFilterColorBalance, I965_RING_VEBOX},
		{VAProcFilterSkinToneEnhancement, I965_RING_VEBOX},
	},
};

static struct hw_codec_info glk_hw_codec_info = {
	.dec_hw_context_init = gen9_dec_hw_context_init,
	.enc_hw_context_init = gen9_enc_hw_context_init,
	.proc_hw_context_init = gen75_proc_context_init,
	.render_init = gen9_render_init,
	.post_processing_context_init = gen9_post_processing_context_init,
	.get_hw_formats = gen8_get_hw_formats,
	.max_resolution = gen9_max_resolution,
	.preinit_hw_codec = gen9_hw_codec_preinit,

	.max_width = 4096,
	.max_height = 4096,

	.max_width_mpeg2 = 1920,
	.max_height_mpeg2 = 1088,

	.max_width_jpeg = 16384,
	.max_height_jpeg = 16384,

	.min_linear_wpitch = 64,
	.min_linear_hpitch = 4,

	.h264_mvc_dec_profiles = (VA_PROFILE_MASK(H264StereoHigh) |
							  VA_PROFILE_MASK(H264MultiviewHigh)),
	.vp9_dec_profiles = VP9_PROFILE_MASK(0) |
						VP9_PROFILE_MASK(2),

	.vp9_enc_profiles = VP9_PROFILE_MASK(0),

	.h264_dec_chroma_formats = EXTRA_H264_DEC_CHROMA_FORMATS,
	.jpeg_dec_chroma_formats = EXTRA_JPEG_DEC_CHROMA_FORMATS,
	.jpeg_enc_chroma_formats = EXTRA_JPEG_ENC_CHROMA_FORMATS,
	.hevc_dec_chroma_formats = EXTRA_HEVC_DEC_CHROMA_FORMATS,
	.vp9_dec_chroma_formats = EXTRA_VP9_DEC_CHROMA_FORMATS,

	.has_mpeg2_decoding = 1,
	.has_h264_decoding = 1,
	.has_h264_encoding = 1,
	.has_vc1_decoding = 1,
	.has_jpeg_decoding = 1,
	.has_jpeg_encoding = 1,
	.has_vpp = 1,
	.has_accelerated_getimage = 1,
	.has_accelerated_putimage = 1,
	.has_tiled_surface = 1,
	.has_di_motion_adptive = 1,
	.has_di_motion_compensated = 1,
	.has_vp8_decoding = 1,
	.has_vp8_encoding = 1,
	.has_h264_mvc_encoding = 1,
	.has_hevc_decoding = 1,
	.has_hevc_encoding = 1,
	.has_hevc10_decoding = 1,
	.has_hevc10_encoding = 1,
	.has_vp9_decoding = 1,
	.has_vpp_p010 = 1,
	.has_vp9_encoding = 1,
	.has_lp_h264_encoding = 1,

	.supports_short_slice_dec = 1,

	.lp_h264_brc_mode = VA_RC_CQP,
	.h264_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR | VA_RC_MB,

	.vp9_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR,

	.num_filters = 5,
	.filters = {
		{VAProcFilterNoiseReduction, I965_RING_VEBOX},
		{VAProcFilterDeinterlacing, I965_RING_VEBOX},
		{VAProcFilterSharpening, I965_RING_NULL},
		{VAProcFilterColorBalance, I965_RING_VEBOX},
		{VAProcFilterSkinToneEnhancement, I965_RING_VEBOX},
	},
};

static struct hw_codec_info cfl_hw_codec_info = {
	.dec_hw_context_init = gen9_dec_hw_context_init,
	.enc_hw_context_init = gen9_enc_hw_context_init,
	.proc_hw_context_init = gen75_proc_context_init,
	.render_init = gen9_render_init,
	.post_processing_context_init = gen9_post_processing_context_init,
	.max_resolution = gen9_max_resolution,
	.preinit_hw_codec = gen9_hw_codec_preinit,
	.get_hw_formats = gen8_get_hw_formats,

	.max_width = 4096,	/* default. See max_resolution */
	.max_height = 4096, /* default. See max_resolution */

	.max_width_mpeg2 = 1920,
	.max_height_mpeg2 = 1088,

	.max_width_jpeg = 16384,
	.max_height_jpeg = 16384,

	.min_linear_wpitch = 64,
	.min_linear_hpitch = 16,

	.h264_mvc_dec_profiles = (VA_PROFILE_MASK(H264StereoHigh) |
							  VA_PROFILE_MASK(H264MultiviewHigh)),
	.vp9_dec_profiles = VP9_PROFILE_MASK(0) |
						VP9_PROFILE_MASK(2),
	.vp9_enc_profiles = VP9_PROFILE_MASK(0),

	.h264_dec_chroma_formats = EXTRA_H264_DEC_CHROMA_FORMATS,
	.jpeg_dec_chroma_formats = EXTRA_JPEG_DEC_CHROMA_FORMATS,
	.jpeg_enc_chroma_formats = EXTRA_JPEG_ENC_CHROMA_FORMATS,
	.hevc_dec_chroma_formats = EXTRA_HEVC_DEC_CHROMA_FORMATS,
	.vp9_dec_chroma_formats = EXTRA_VP9_DEC_CHROMA_FORMATS,

	.has_mpeg2_decoding = 1,
	.has_mpeg2_encoding = 1,
	.has_h264_decoding = 1,
	.has_h264_encoding = 1,
	.has_vc1_decoding = 1,
	.has_jpeg_decoding = 1,
	.has_jpeg_encoding = 1,
	.has_vpp = 1,
	.has_accelerated_getimage = 1,
	.has_accelerated_putimage = 1,
	.has_tiled_surface = 1,
	.has_di_motion_adptive = 1,
	.has_di_motion_compensated = 1,
	.has_vp8_decoding = 1,
	.has_vp8_encoding = 1,
	.has_h264_mvc_encoding = 1,
	.has_hevc_decoding = 1,
	.has_hevc_encoding = 1,
	.has_hevc10_encoding = 1,
	.has_hevc10_decoding = 1,
	.has_vp9_decoding = 1,
	.has_vpp_p010 = 1,
	.has_vp9_encoding = 1,
	.has_lp_h264_encoding = 1,

	.supports_short_slice_dec = 1,

	.lp_h264_brc_mode = VA_RC_CQP,
	.h264_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR | VA_RC_MB,

	.vp9_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR,

	.num_filters = 5,
	.filters = {
		{VAProcFilterNoiseReduction, I965_RING_VEBOX},
		{VAProcFilterDeinterlacing, I965_RING_VEBOX},
		{VAProcFilterSharpening, I965_RING_NULL},
		{VAProcFilterColorBalance, I965_RING_VEBOX},
		{VAProcFilterSkinToneEnhancement, I965_RING_VEBOX},
	},
};

static struct hw_codec_info cnl_hw_codec_info = {
	.dec_hw_context_init = gen9_dec_hw_context_init,
	.enc_hw_context_init = gen9_enc_hw_context_init,
	.proc_hw_context_init = gen75_proc_context_init,
	.render_init = gen9_render_init,
	.post_processing_context_init = gen9_post_processing_context_init,
	.max_resolution = gen9_max_resolution,
	.preinit_hw_codec = gen9_hw_codec_preinit,
	.get_hw_formats = gen8_get_hw_formats,

	.max_width = 4096,	/* default. See max_resolution */
	.max_height = 4096, /* default. See max_resolution */

	.max_width_mpeg2 = 1920,
	.max_height_mpeg2 = 1088,

	.max_width_jpeg = 16384,
	.max_height_jpeg = 16384,

	.min_linear_wpitch = 64,
	.min_linear_hpitch = 16,

	.h264_mvc_dec_profiles = (VA_PROFILE_MASK(H264StereoHigh) | VA_PROFILE_MASK(H264MultiviewHigh)),
	.vp9_dec_profiles = VP9_PROFILE_MASK(0) | VP9_PROFILE_MASK(2),
	.vp9_enc_profiles = VP9_PROFILE_MASK(0),

	.h264_dec_chroma_formats = EXTRA_H264_DEC_CHROMA_FORMATS,
	.jpeg_dec_chroma_formats = EXTRA_JPEG_DEC_CHROMA_FORMATS,
	.jpeg_enc_chroma_formats = EXTRA_JPEG_ENC_CHROMA_FORMATS,
	.hevc_dec_chroma_formats = EXTRA_HEVC_DEC_CHROMA_FORMATS,
	.vp9_dec_chroma_formats = EXTRA_VP9_DEC_CHROMA_FORMATS,

	.has_mpeg2_decoding = 1,
	.has_mpeg2_encoding = 1,
	.has_h264_decoding = 1,
	.has_h264_encoding = 1,
	.has_vc1_decoding = 1,
	.has_jpeg_decoding = 1,
	.has_jpeg_encoding = 1,
	.has_vpp = 1,
	.has_accelerated_getimage = 1,
	.has_accelerated_putimage = 1,
	.has_tiled_surface = 1,
	.has_di_motion_adptive = 1,
	.has_di_motion_compensated = 1,
	.has_vp8_decoding = 1,
	.has_vp8_encoding = 1,
	.has_h264_mvc_encoding = 1,
	.has_hevc_decoding = 1,
	.has_hevc_encoding = 1,
	.has_hevc10_decoding = 1,
	.has_hevc10_encoding = 1,
	.has_vp9_decoding = 1,
	.has_vpp_p010 = 1,
	.has_vp9_encoding = 1,
	.has_lp_h264_encoding = 1,
	.has_lp_vp9_encoding = 1,

	.lp_h264_brc_mode = VA_RC_CQP,
	.h264_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR | VA_RC_MB,

	.vp9_brc_mode = VA_RC_CQP | VA_RC_CBR | VA_RC_VBR,

	.num_filters = 5,
	.filters = {
		{VAProcFilterNoiseReduction, I965_RING_VEBOX},
		{VAProcFilterDeinterlacing, I965_RING_VEBOX},
		{VAProcFilterSharpening, I965_RING_NULL},
		{VAProcFilterColorBalance, I965_RING_VEBOX},
		{VAProcFilterSkinToneEnhancement, I965_RING_VEBOX},
	},
};

struct hw_codec_info *
i965_get_codec_info(int devid)
{
	switch (devid) {
#undef CHIPSET
#define CHIPSET(id, family, dev, str) case id: return &family##_hw_codec_info;
#include "i965_pciids.h"
	default:
		return NULL;
	}
}

static const struct intel_device_info g4x_device_info = {
	.gen = 4,

	.urb_size = 384,
	.max_wm_threads = 50,       /* 10 * 5 */

	.is_g4x = 1,
};

static const struct intel_device_info ilk_device_info = {
	.gen = 5,

	.urb_size = 1024,
	.max_wm_threads = 72,       /* 12 * 6 */

	.driver_workarounds = HW_WORKAROUND_INVALID_RGBX_SHADER_USE
};

static const struct intel_device_info snb_gt1_device_info = {
	.gen = 6,
	.gt = 1,

	.urb_size = 1024,
	.max_wm_threads = 40,

	.driver_workarounds = HW_WORKAROUND_INVALID_RGBX_SHADER_USE
};

static const struct intel_device_info snb_gt2_device_info = {
	.gen = 6,
	.gt = 2,

	.urb_size = 1024,
	.max_wm_threads = 80,

	.driver_workarounds = HW_WORKAROUND_INVALID_RGBX_SHADER_USE
};

static const struct intel_device_info ivb_gt1_device_info = {
	.gen = 7,
	.gt = 1,

	.urb_size = 4096,
	.max_wm_threads = 48,

	.is_ivybridge = 1,
};

static const struct intel_device_info ivb_gt2_device_info = {
	.gen = 7,
	.gt = 2,

	.urb_size = 4096,
	.max_wm_threads = 172,

	.is_ivybridge = 1,
};

static const struct intel_device_info byt_device_info = {
	.gen = 7,
	.gt = 1,

	.urb_size = 4096,
	.max_wm_threads = 48,

	.is_ivybridge = 1,
	.is_baytrail = 1,
};

static const struct intel_device_info hsw_gt1_device_info = {
	.gen = 7,
	.gt = 1,

	.urb_size = 4096,
	.max_wm_threads = 102,

	.is_haswell = 1,
};

static const struct intel_device_info hsw_gt2_device_info = {
	.gen = 7,
	.gt = 2,

	.urb_size = 4096,
	.max_wm_threads = 204,

	.is_haswell = 1,
};

static const struct intel_device_info hsw_gt3_device_info = {
	.gen = 7,
	.gt = 3,

	.urb_size = 4096,
	.max_wm_threads = 408,

	.is_haswell = 1,
};

static const struct intel_device_info bdw_device_info = {
	.gen = 8,

	.urb_size = 4096,
	.max_wm_threads = 64,       /* per PSD */
};

static const struct intel_device_info chv_device_info = {
	.gen = 8,

	.urb_size = 4096,
	.max_wm_threads = 64,       /* per PSD */

	.driver_workarounds = HW_WORKAROUND_USE_ALTERNATE_SEARCH_TABLE,

	.is_cherryview = 1,
};

static const struct intel_device_info skl_device_info = {
	.gen = 9,

	.urb_size = 4096,
	.max_wm_threads = 64,       /* per PSD */

	.is_skylake = 1,
};

static const struct intel_device_info bxt_device_info = {
	.gen = 9,

	.urb_size = 4096,
	.max_wm_threads = 64,       /* per PSD */

	.driver_workarounds = HW_WORKAROUND_USE_ALTERNATE_SEARCH_TABLE,

	.is_broxton = 1,
};

static const struct intel_device_info kbl_device_info = {
	.gen = 9,

	.urb_size = 4096,
	.max_wm_threads = 64,       /* per PSD */

	.is_kabylake = 1,
};

static const struct intel_device_info glk_device_info = {
	.gen = 9,

	.urb_size = 4096,
	.max_wm_threads = 64,       /* per PSD */

	.driver_workarounds = HW_WORKAROUND_USE_ALTERNATE_SEARCH_TABLE,

	.is_glklake = 1,
};

static const struct intel_device_info cfl_device_info = {
	.gen = 9,

	.urb_size = 4096,
	.max_wm_threads = 64,       /* per PSD */

	.is_cfllake = 1,
};

static const struct intel_device_info cnl_device_info = {
	.gen = 10,

	.urb_size = 4096,
	.max_wm_threads = 64,       /* per PSD */
};

const struct intel_device_info *
i965_get_device_info(int devid)
{
	switch (devid) {
#undef CHIPSET
#define CHIPSET(id, family, dev, str) case id: return &dev##_device_info;
#include "i965_pciids.h"
	default:
		return NULL;
	}
}

static void cpuid(unsigned int op,
				  uint32_t *eax, uint32_t *ebx,
				  uint32_t *ecx, uint32_t *edx)
{
	__cpuid_count(op, 0, *eax, *ebx, *ecx, *edx);
}

/*
 * This function doesn't check the length. And the caller should
 * assure that the length of input string should be greater than 48.
 */
static int intel_driver_detect_cpustring(char *model_id)
{
	uint32_t *rdata;

	if (model_id == NULL)
		return -EINVAL;

	rdata = (uint32_t *)model_id;

	/* obtain the max supported extended CPUID info */
	cpuid(0x80000000, &rdata[0], &rdata[1], &rdata[2], &rdata[3]);

	/* If the max extended CPUID info is less than 0x80000004, fail */
	if (rdata[0] < 0x80000004)
		return -EINVAL;

	/* obtain the CPUID string */
	cpuid(0x80000002, &rdata[0], &rdata[1], &rdata[2], &rdata[3]);
	cpuid(0x80000003, &rdata[4], &rdata[5], &rdata[6], &rdata[7]);
	cpuid(0x80000004, &rdata[8], &rdata[9], &rdata[10], &rdata[11]);

	*(model_id + 48) = '\0';
	return 0;
}

/*
 * the hook_list for HSW.
 * It is captured by /proc/cpuinfo and the space character is stripped.
 */
const static char *hsw_cpu_hook_list[] =  {
	"Intel(R)Pentium(R)3556U",
	"Intel(R)Pentium(R)3560Y",
	"Intel(R)Pentium(R)3550M",
	"Intel(R)Celeron(R)2980U",
	"Intel(R)Celeron(R)2955U",
	"Intel(R)Celeron(R)2950M",
};

static void hsw_hw_codec_preinit(VADriverContextP ctx, struct hw_codec_info *codec_info)
{
	char model_string[64];
	char *model_ptr, *tmp_ptr;
	int i, model_len, list_len;
	bool found;

	memset(model_string, 0, sizeof(model_string));

	/* If it can't detect cpu model_string, leave it alone */
	if (intel_driver_detect_cpustring(model_string))
		return;

	/* strip the cpufreq info */
	model_ptr = model_string;
	tmp_ptr = strstr(model_ptr, "@");

	if (tmp_ptr)
		*tmp_ptr = '\0';

	/* strip the space character and convert to the lower case */
	model_ptr = model_string;
	model_len = strlen(model_string);
	for (i = 0; i < model_len; i++) {
		if (model_string[i] != ' ') {
			*model_ptr = model_string[i];
			model_ptr++;
		}
	}
	*model_ptr = '\0';

	found = false;
	list_len = sizeof(hsw_cpu_hook_list) / sizeof(char *);
	model_len = strlen(model_string);
	for (i = 0; i < list_len; i++) {
		model_ptr = (char *)hsw_cpu_hook_list[i];

		if (strlen(model_ptr) != model_len)
			continue;

		if (strncasecmp(model_string, model_ptr, model_len) == 0) {
			found = true;
			break;
		}
	}

	if (found) {
		codec_info->has_h264_encoding = 0;
		codec_info->has_h264_mvc_encoding = 0;
		codec_info->has_mpeg2_encoding = 0;
	}
	return;
}

/*
 * the hook_list for Sandybride.
 * It is captured by /proc/cpuinfo and the space character is stripped.
 */
const static char *gen6_cpu_hook_list[] =  {
	"Intel(R)Celeron(R)CPU847",
	"Intel(R)Celeron(R)CPU867",
};

static void gen6_hw_codec_preinit(VADriverContextP ctx, struct hw_codec_info *codec_info)
{
	char model_string[64];
	char *model_ptr, *tmp_ptr;
	int i, model_len, list_len;
	bool found;

	memset(model_string, 0, sizeof(model_string));

	/* If it can't detect cpu model_string, leave it alone */
	if (intel_driver_detect_cpustring(model_string))
		return;

	/* strip the cpufreq info */
	model_ptr = model_string;
	tmp_ptr = strstr(model_ptr, "@");

	if (tmp_ptr)
		*tmp_ptr = '\0';

	/* strip the space character and convert to the lower case */
	model_ptr = model_string;
	model_len = strlen(model_string);
	for (i = 0; i < model_len; i++) {
		if (model_string[i] != ' ') {
			*model_ptr = model_string[i];
			model_ptr++;
		}
	}
	*model_ptr = '\0';

	found = false;
	list_len = sizeof(gen6_cpu_hook_list) / sizeof(char *);
	model_len = strlen(model_string);
	for (i = 0; i < list_len; i++) {
		model_ptr = (char *)gen6_cpu_hook_list[i];

		if (strlen(model_ptr) != model_len)
			continue;

		if (strncasecmp(model_string, model_ptr, model_len) == 0) {
			found = true;
			break;
		}
	}

	if (found) {
		codec_info->has_h264_encoding = 0;
	}
	return;
}

/*
 * the hook_list for Ivybridge.
 * It is captured by /proc/cpuinfo and the space character is stripped.
 */
const static char *gen7_cpu_hook_list[] =  {
	"Intel(R)Celeron(R)CPUG1620T",
	"Intel(R)Celeron(R)CPUG1630",
	"Intel(R)Celeron(R)CPU1005M",
	"Intel(R)Celeron(R)CPU1017U",
	"Intel(R)Celeron(R)CPU1019Y",
	"Intel(R)Celeron(R)CPU1000M",
	"Intel(R)Celeron(R)CPU1007U",
	"Intel(R)Celeron(R)CPU1020E",
	"Intel(R)Celeron(R)CPU1020M",
	"Intel(R)Celeron(R)CPU1037U",
	"Intel(R)Celeron(R)CPU1047UE",
	"Intel(R)Celeron(R)CPU927UE",
	"Intel(R)Celeron(R)CPUG1610",
	"Intel(R)Celeron(R)CPUG1610T",
	"Intel(R)Celeron(R)CPUG1620",
	"Intel(R)Pentium(R)CPU2020M",
	"Intel(R)Pentium(R)CPU2030M",
	"Intel(R)Pentium(R)CPU2117U",
	"Intel(R)Pentium(R)CPU2127U",
	"Intel(R)Pentium(R)CPU2129Y",
	"Intel(R)Pentium(R)CPUA1018",
	"Intel(R)Pentium(R)CPUG2010",
	"Intel(R)Pentium(R)CPUG2020",
	"Intel(R)Pentium(R)CPUG2020T",
	"Intel(R)Pentium(R)CPUG2030",
	"Intel(R)Pentium(R)CPUG2030T",
	"Intel(R)Pentium(R)CPUG2100T",
	"Intel(R)Pentium(R)CPUG2120",
	"Intel(R)Pentium(R)CPUG2120T",
	"Intel(R)Pentium(R)CPUG2130",
	"Intel(R)Pentium(R)CPUG2140",
};

static void gen7_hw_codec_preinit(VADriverContextP ctx, struct hw_codec_info *codec_info)
{
	char model_string[64];
	char *model_ptr, *tmp_ptr;
	int i, model_len, list_len;
	bool found;

	memset(model_string, 0, sizeof(model_string));

	/* If it can't detect cpu model_string, leave it alone */
	if (intel_driver_detect_cpustring(model_string))
		return;

	/* strip the cpufreq info */
	model_ptr = model_string;
	tmp_ptr = strstr(model_ptr, "@");

	if (tmp_ptr)
		*tmp_ptr = '\0';

	/* strip the space character and convert to the lower case */
	model_ptr = model_string;
	model_len = strlen(model_string);
	for (i = 0; i < model_len; i++) {
		if (model_string[i] != ' ') {
			*model_ptr = model_string[i];
			model_ptr++;
		}
	}
	*model_ptr = '\0';

	found = false;
	list_len = sizeof(gen7_cpu_hook_list) / sizeof(char *);
	model_len = strlen(model_string);
	for (i = 0; i < list_len; i++) {
		model_ptr = (char *)gen7_cpu_hook_list[i];

		if (strlen(model_ptr) != model_len)
			continue;

		if (strncasecmp(model_string, model_ptr, model_len) == 0) {
			found = true;
			break;
		}
	}

	if (found) {
		codec_info->has_h264_encoding = 0;
		codec_info->has_mpeg2_encoding = 0;
	}
	return;
}

static void gen9_hw_codec_preinit(VADriverContextP ctx, struct hw_codec_info *codec_info)
{
	struct i965_driver_data *i965 = i965_driver_data(ctx);

	if (i965->intel.has_huc && codec_info->has_lp_h264_encoding)
		codec_info->lp_h264_brc_mode |= (VA_RC_CBR | VA_RC_VBR);

	if (i965->intel.has_huc && codec_info->has_lp_vp9_encoding)
		codec_info->lp_vp9_brc_mode |= (VA_RC_CQP | VA_RC_CBR | VA_RC_VBR);
}
