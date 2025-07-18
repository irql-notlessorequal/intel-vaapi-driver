/*
 * Copyright (C) 2011 Intel Corporation
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
 * Authors:
 *    Xiang Haihao <haihao.xiang@intel.com>
 *    Zhao  Yakui  <yakui.zhao@intel.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <va/va_dec_jpeg.h>
#include <va/va_dec_vp8.h>

#include "intel_batchbuffer.h"
#include "intel_driver.h"

#include "i965_defines.h"
#include "i965_drv_video.h"
#include "i965_decoder_utils.h"

#include "gen7_mfd.h"
#include "intel_media.h"

#define B0_STEP_REV 2
#define IS_STEPPING_BPLUS(i965) ((i965->intel.revision) >= B0_STEP_REV)

static const uint32_t zigzag_direct[64] = {
	0,   1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

static void
gen8_mfd_init_avc_surface(VADriverContextP ctx,
						  VAPictureParameterBufferH264 *pic_param,
						  struct object_surface *obj_surface)
{
	struct i965_driver_data *i965 = i965_driver_data(ctx);
	GenAvcSurface *gen7_avc_surface = obj_surface->private_data;
	int width_in_mbs, height_in_mbs;

	obj_surface->free_private_data = gen_free_avc_surface;
	width_in_mbs = pic_param->picture_width_in_mbs_minus1 + 1;
	height_in_mbs = pic_param->picture_height_in_mbs_minus1 + 1; /* frame height */

	if (!gen7_avc_surface) {
		gen7_avc_surface = calloc(sizeof(GenAvcSurface), 1);

		if (!gen7_avc_surface)
			return;

		gen7_avc_surface->base.frame_store_id = -1;
		assert((obj_surface->size & 0x3f) == 0);
		obj_surface->private_data = gen7_avc_surface;
	}

	/* DMV buffers now relate to the whole frame, irrespective of
	   field coding modes */
	if (gen7_avc_surface->dmv_top == NULL) {
		gen7_avc_surface->dmv_top = dri_bo_alloc(i965->intel.bufmgr,
												 "direct mv w/r buffer",
												 width_in_mbs * height_in_mbs * 128,
												 0x1000);
		assert(gen7_avc_surface->dmv_top);
	}
}

static void
gen8_mfd_pipe_mode_select(VADriverContextP ctx,
						  struct decode_state *decode_state,
						  int standard_select,
						  struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;

	assert(standard_select == MFX_FORMAT_MPEG2 ||
		   standard_select == MFX_FORMAT_AVC ||
		   standard_select == MFX_FORMAT_VC1 ||
		   standard_select == MFX_FORMAT_JPEG ||
		   standard_select == MFX_FORMAT_VP8);

	BEGIN_BCS_BATCH(batch, 5);
	OUT_BCS_BATCH(batch, MFX_PIPE_MODE_SELECT | (5 - 2));
	OUT_BCS_BATCH(batch,
				  (gen7_mfd_context->decoder_format_mode << 17) | /* Currently only support long format */
				  (MFD_MODE_VLD << 15) | /* VLD mode */
				  (0 << 10) | /* disable Stream-Out */
				  (gen7_mfd_context->post_deblocking_output.valid << 9)  | /* Post Deblocking Output */
				  (gen7_mfd_context->pre_deblocking_output.valid << 8)  | /* Pre Deblocking Output */
				  (0 << 5)  | /* not in stitch mode */
				  (MFX_CODEC_DECODE << 4)  | /* decoding mode */
				  (standard_select << 0));
	OUT_BCS_BATCH(batch,
				  (0 << 4)  | /* terminate if AVC motion and POC table error occurs */
				  (0 << 3)  | /* terminate if AVC mbdata error occurs */
				  (0 << 2)  | /* terminate if AVC CABAC/CAVLC decode error occurs */
				  (0 << 1)  |
				  (0 << 0));
	OUT_BCS_BATCH(batch, 0); /* pic status/error report id */
	OUT_BCS_BATCH(batch, 0); /* reserved */
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_surface_state(VADriverContextP ctx,
					   struct decode_state *decode_state,
					   int standard_select,
					   struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	struct object_surface *obj_surface = decode_state->render_object;
	unsigned int y_cb_offset;
	unsigned int y_cr_offset;
	unsigned int surface_format;

	assert(obj_surface);

	y_cb_offset = obj_surface->y_cb_offset;
	y_cr_offset = obj_surface->y_cr_offset;

	surface_format = obj_surface->fourcc == VA_FOURCC_Y800 ?
					 MFX_SURFACE_MONOCHROME : MFX_SURFACE_PLANAR_420_8;

	BEGIN_BCS_BATCH(batch, 6);
	OUT_BCS_BATCH(batch, MFX_SURFACE_STATE | (6 - 2));
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch,
				  ((obj_surface->orig_height - 1) << 18) |
				  ((obj_surface->orig_width - 1) << 4));
	OUT_BCS_BATCH(batch,
				  (surface_format << 28) | /* 420 planar YUV surface */
				  ((standard_select != MFX_FORMAT_JPEG) << 27) | /* interleave chroma, set to 0 for JPEG */
				  (0 << 22) | /* surface object control state, ignored */
				  ((obj_surface->width - 1) << 3) | /* pitch */
				  (0 << 2)  | /* must be 0 */
				  (1 << 1)  | /* must be tiled */
				  (I965_TILEWALK_YMAJOR << 0));  /* tile walk, must be 1 */
	OUT_BCS_BATCH(batch,
				  (0 << 16) | /* X offset for U(Cb), must be 0 */
				  (y_cb_offset << 0)); /* Y offset for U(Cb) */
	OUT_BCS_BATCH(batch,
				  (0 << 16) | /* X offset for V(Cr), must be 0 */
				  ((standard_select == MFX_FORMAT_JPEG ? y_cr_offset : 0) << 0)); /* Y offset for V(Cr), must be 0 for video codec, non-zoro for JPEG */
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_pipe_buf_addr_state(VADriverContextP ctx,
							 struct decode_state *decode_state,
							 int standard_select,
							 struct gen7_mfd_context *gen7_mfd_context)
{
	struct i965_driver_data *i965 = i965_driver_data(ctx);
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	int i;

	BEGIN_BCS_BATCH(batch, 61);
	OUT_BCS_BATCH(batch, MFX_PIPE_BUF_ADDR_STATE | (61 - 2));
	/* Pre-deblock 1-3 */
	if (gen7_mfd_context->pre_deblocking_output.valid)
		OUT_BCS_RELOC64(batch, gen7_mfd_context->pre_deblocking_output.bo,
						I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
						0);
	else {
		OUT_BCS_BATCH(batch, 0);

		OUT_BCS_BATCH(batch, 0);
	}
	OUT_BCS_BATCH(batch, i965->intel.mocs_state);

	/* Post-debloing 4-6 */
	if (gen7_mfd_context->post_deblocking_output.valid)
		OUT_BCS_RELOC64(batch, gen7_mfd_context->post_deblocking_output.bo,
						I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
						0);
	else {
		OUT_BCS_BATCH(batch, 0);

		OUT_BCS_BATCH(batch, 0);
	}
	OUT_BCS_BATCH(batch, i965->intel.mocs_state);

	/* uncompressed-video & stream out 7-12 */
	OUT_BCS_BATCH(batch, 0); /* ignore for decoding */
	OUT_BCS_BATCH(batch, 0); /* ignore for decoding */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	/* intra row-store scratch 13-15 */
	if (gen7_mfd_context->intra_row_store_scratch_buffer.valid)
		OUT_BCS_RELOC64(batch, gen7_mfd_context->intra_row_store_scratch_buffer.bo,
						I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
						0);
	else {
		OUT_BCS_BATCH(batch, 0);

		OUT_BCS_BATCH(batch, 0);
	}
	OUT_BCS_BATCH(batch, i965->intel.mocs_state);

	/* deblocking-filter-row-store 16-18 */
	if (gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.valid)
		OUT_BCS_RELOC64(batch, gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.bo,
						I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
						0);
	else {
		OUT_BCS_BATCH(batch, 0);
		OUT_BCS_BATCH(batch, 0);
	}

	OUT_BCS_BATCH(batch, i965->intel.mocs_state);

	/* DW 19..50 */
	for (i = 0; i < ARRAY_ELEMS(gen7_mfd_context->reference_surface); i++) {
		struct object_surface *obj_surface;

		if (gen7_mfd_context->reference_surface[i].surface_id != VA_INVALID_ID &&
			gen7_mfd_context->reference_surface[i].obj_surface &&
			gen7_mfd_context->reference_surface[i].obj_surface->bo) {
			obj_surface = gen7_mfd_context->reference_surface[i].obj_surface;

			OUT_BCS_RELOC64(batch, obj_surface->bo,
							I915_GEM_DOMAIN_INSTRUCTION, 0,
							0);
		} else {
			OUT_BCS_BATCH(batch, 0);
			OUT_BCS_BATCH(batch, 0);
		}

	}

	/* reference property 51 */
	OUT_BCS_BATCH(batch, i965->intel.mocs_state);

	/* Macroblock status & ILDB 52-57 */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	/* the second Macroblock status 58-60 */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_ind_obj_base_addr_state(VADriverContextP ctx,
								 dri_bo *slice_data_bo,
								 int standard_select,
								 struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	struct i965_driver_data *i965 = i965_driver_data(ctx);

	BEGIN_BCS_BATCH(batch, 26);
	OUT_BCS_BATCH(batch, MFX_IND_OBJ_BASE_ADDR_STATE | (26 - 2));
	/* MFX In BS 1-5 */
	OUT_BCS_RELOC64(batch, slice_data_bo, I915_GEM_DOMAIN_INSTRUCTION, 0, 0); /* MFX Indirect Bitstream Object Base Address */
	OUT_BCS_BATCH(batch, i965->intel.mocs_state);
	/* Upper bound 4-5 */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	/* MFX indirect MV 6-10 */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	/* MFX IT_COFF 11-15 */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	/* MFX IT_DBLK 16-20 */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	/* MFX PAK_BSE object for encoder 21-25 */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_bsp_buf_base_addr_state(VADriverContextP ctx,
								 struct decode_state *decode_state,
								 int standard_select,
								 struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	struct i965_driver_data *i965 = i965_driver_data(ctx);

	BEGIN_BCS_BATCH(batch, 10);
	OUT_BCS_BATCH(batch, MFX_BSP_BUF_BASE_ADDR_STATE | (10 - 2));

	if (gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.valid)
		OUT_BCS_RELOC64(batch, gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.bo,
						I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
						0);
	else {
		OUT_BCS_BATCH(batch, 0);
		OUT_BCS_BATCH(batch, 0);
	}

	OUT_BCS_BATCH(batch, i965->intel.mocs_state);
	/* MPR Row Store Scratch buffer 4-6 */
	if (gen7_mfd_context->mpr_row_store_scratch_buffer.valid)
		OUT_BCS_RELOC64(batch, gen7_mfd_context->mpr_row_store_scratch_buffer.bo,
						I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
						0);
	else {
		OUT_BCS_BATCH(batch, 0);
		OUT_BCS_BATCH(batch, 0);
	}

	OUT_BCS_BATCH(batch, i965->intel.mocs_state);

	/* Bitplane 7-9 */
	if (gen7_mfd_context->bitplane_read_buffer.valid)
		OUT_BCS_RELOC64(batch, gen7_mfd_context->bitplane_read_buffer.bo,
						I915_GEM_DOMAIN_INSTRUCTION, 0,
						0);
	else {
		OUT_BCS_BATCH(batch, 0);
		OUT_BCS_BATCH(batch, 0);
	}
	OUT_BCS_BATCH(batch, i965->intel.mocs_state);
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_qm_state(VADriverContextP ctx,
				  int qm_type,
				  unsigned char *qm,
				  int qm_length,
				  struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	unsigned int qm_buffer[16];

	assert(qm_length <= 16 * 4);
	memcpy(qm_buffer, qm, qm_length);

	BEGIN_BCS_BATCH(batch, 18);
	OUT_BCS_BATCH(batch, MFX_QM_STATE | (18 - 2));
	OUT_BCS_BATCH(batch, qm_type << 0);
	intel_batchbuffer_data(batch, qm_buffer, 16 * 4);
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_avc_img_state(VADriverContextP ctx,
					   struct decode_state *decode_state,
					   struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	int img_struct;
	int mbaff_frame_flag;
	unsigned int width_in_mbs, height_in_mbs;
	VAPictureParameterBufferH264 *pic_param;

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferH264 *)decode_state->pic_param->buffer;
	assert(!(pic_param->CurrPic.flags & VA_PICTURE_H264_INVALID));

	if (pic_param->CurrPic.flags & VA_PICTURE_H264_TOP_FIELD)
		img_struct = 1;
	else if (pic_param->CurrPic.flags & VA_PICTURE_H264_BOTTOM_FIELD)
		img_struct = 3;
	else
		img_struct = 0;

	if ((img_struct & 0x1) == 0x1) {
		assert(pic_param->pic_fields.bits.field_pic_flag == 0x1);
	} else {
		assert(pic_param->pic_fields.bits.field_pic_flag == 0x0);
	}

	if (pic_param->seq_fields.bits.frame_mbs_only_flag) { /* a frame containing only frame macroblocks */
		assert(pic_param->seq_fields.bits.mb_adaptive_frame_field_flag == 0);
		assert(pic_param->pic_fields.bits.field_pic_flag == 0);
	} else {
		assert(pic_param->seq_fields.bits.direct_8x8_inference_flag == 1); /* see H.264 spec */
	}

	mbaff_frame_flag = (pic_param->seq_fields.bits.mb_adaptive_frame_field_flag &&
						!pic_param->pic_fields.bits.field_pic_flag);

	width_in_mbs = pic_param->picture_width_in_mbs_minus1 + 1;
	height_in_mbs = pic_param->picture_height_in_mbs_minus1 + 1; /* frame height */

	/* MFX unit doesn't support 4:2:2 and 4:4:4 picture */
	assert(pic_param->seq_fields.bits.chroma_format_idc == 0 || /* monochrome picture */
		   pic_param->seq_fields.bits.chroma_format_idc == 1);  /* 4:2:0 */
	assert(pic_param->seq_fields.bits.residual_colour_transform_flag == 0); /* only available for 4:4:4 */

	BEGIN_BCS_BATCH(batch, 17);
	OUT_BCS_BATCH(batch, MFX_AVC_IMG_STATE | (17 - 2));
	OUT_BCS_BATCH(batch,
				  (width_in_mbs * height_in_mbs - 1));
	OUT_BCS_BATCH(batch,
				  ((height_in_mbs - 1) << 16) |
				  ((width_in_mbs - 1) << 0));
	OUT_BCS_BATCH(batch,
				  ((pic_param->second_chroma_qp_index_offset & 0x1f) << 24) |
				  ((pic_param->chroma_qp_index_offset & 0x1f) << 16) |
				  (0 << 14) | /* Max-bit conformance Intra flag ??? FIXME */
				  (0 << 13) | /* Max Macroblock size conformance Inter flag ??? FIXME */
				  (pic_param->pic_fields.bits.weighted_pred_flag << 12) | /* differ from GEN6 */
				  (pic_param->pic_fields.bits.weighted_bipred_idc << 10) |
				  (img_struct << 8));
	OUT_BCS_BATCH(batch,
				  (pic_param->seq_fields.bits.chroma_format_idc << 10) |
				  (pic_param->pic_fields.bits.entropy_coding_mode_flag << 7) |
				  ((!pic_param->pic_fields.bits.reference_pic_flag) << 6) |
				  (pic_param->pic_fields.bits.constrained_intra_pred_flag << 5) |
				  (pic_param->seq_fields.bits.direct_8x8_inference_flag << 4) |
				  (pic_param->pic_fields.bits.transform_8x8_mode_flag << 3) |
				  (pic_param->seq_fields.bits.frame_mbs_only_flag << 2) |
				  (mbaff_frame_flag << 1) |
				  (pic_param->pic_fields.bits.field_pic_flag << 0));
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_avc_qm_state(VADriverContextP ctx,
					  struct decode_state *decode_state,
					  struct gen7_mfd_context *gen7_mfd_context)
{
	VAIQMatrixBufferH264 *iq_matrix;
	VAPictureParameterBufferH264 *pic_param;

	if (decode_state->iq_matrix && decode_state->iq_matrix->buffer)
		iq_matrix = (VAIQMatrixBufferH264 *)decode_state->iq_matrix->buffer;
	else
		iq_matrix = &gen7_mfd_context->iq_matrix.h264;

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferH264 *)decode_state->pic_param->buffer;

	gen8_mfd_qm_state(ctx, MFX_QM_AVC_4X4_INTRA_MATRIX, &iq_matrix->ScalingList4x4[0][0], 3 * 16, gen7_mfd_context);
	gen8_mfd_qm_state(ctx, MFX_QM_AVC_4X4_INTER_MATRIX, &iq_matrix->ScalingList4x4[3][0], 3 * 16, gen7_mfd_context);

	if (pic_param->pic_fields.bits.transform_8x8_mode_flag) {
		gen8_mfd_qm_state(ctx, MFX_QM_AVC_8x8_INTRA_MATRIX, &iq_matrix->ScalingList8x8[0][0], 64, gen7_mfd_context);
		gen8_mfd_qm_state(ctx, MFX_QM_AVC_8x8_INTER_MATRIX, &iq_matrix->ScalingList8x8[1][0], 64, gen7_mfd_context);
	}
}

static inline void
gen8_mfd_avc_picid_state(VADriverContextP ctx,
						 struct decode_state *decode_state,
						 struct gen7_mfd_context *gen7_mfd_context)
{
	gen75_send_avc_picid_state(gen7_mfd_context->base.batch,
							   gen7_mfd_context->reference_surface);
}

static void
gen8_mfd_avc_directmode_state(VADriverContextP ctx,
							  struct decode_state *decode_state,
							  VAPictureParameterBufferH264 *pic_param,
							  VASliceParameterBufferH264 *slice_param,
							  struct gen7_mfd_context *gen7_mfd_context)
{
	struct i965_driver_data *i965 = i965_driver_data(ctx);
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	struct object_surface *obj_surface;
	GenAvcSurface *gen7_avc_surface;
	VAPictureH264 *va_pic;
	int i;

	BEGIN_BCS_BATCH(batch, 71);
	OUT_BCS_BATCH(batch, MFX_AVC_DIRECTMODE_STATE | (71 - 2));

	/* reference surfaces 0..15 */
	for (i = 0; i < ARRAY_ELEMS(gen7_mfd_context->reference_surface); i++) {
		if (gen7_mfd_context->reference_surface[i].surface_id != VA_INVALID_ID &&
			gen7_mfd_context->reference_surface[i].obj_surface &&
			gen7_mfd_context->reference_surface[i].obj_surface->private_data) {

			obj_surface = gen7_mfd_context->reference_surface[i].obj_surface;
			gen7_avc_surface = obj_surface->private_data;

			OUT_BCS_RELOC64(batch, gen7_avc_surface->dmv_top,
							I915_GEM_DOMAIN_INSTRUCTION, 0,
							0);
		} else {
			OUT_BCS_BATCH(batch, 0);
			OUT_BCS_BATCH(batch, 0);
		}
	}

	OUT_BCS_BATCH(batch, i965->intel.mocs_state);

	/* the current decoding frame/field */
	va_pic = &pic_param->CurrPic;
	obj_surface = decode_state->render_object;
	assert(obj_surface->bo && obj_surface->private_data);
	gen7_avc_surface = obj_surface->private_data;

	OUT_BCS_RELOC64(batch, gen7_avc_surface->dmv_top,
					I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
					0);

	OUT_BCS_BATCH(batch, i965->intel.mocs_state);

	/* POC List */
	for (i = 0; i < ARRAY_ELEMS(gen7_mfd_context->reference_surface); i++) {
		obj_surface = gen7_mfd_context->reference_surface[i].obj_surface;

		if (obj_surface) {
			const VAPictureH264 * const va_pic = avc_find_picture(
													 obj_surface->base.id, pic_param->ReferenceFrames,
													 ARRAY_ELEMS(pic_param->ReferenceFrames));

			assert(va_pic != NULL);
			OUT_BCS_BATCH(batch, va_pic->TopFieldOrderCnt);
			OUT_BCS_BATCH(batch, va_pic->BottomFieldOrderCnt);
		} else {
			OUT_BCS_BATCH(batch, 0);
			OUT_BCS_BATCH(batch, 0);
		}
	}

	va_pic = &pic_param->CurrPic;
	OUT_BCS_BATCH(batch, va_pic->TopFieldOrderCnt);
	OUT_BCS_BATCH(batch, va_pic->BottomFieldOrderCnt);

	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_avc_phantom_slice_first(VADriverContextP ctx,
								 VAPictureParameterBufferH264 *pic_param,
								 VASliceParameterBufferH264 *next_slice_param,
								 struct gen7_mfd_context *gen7_mfd_context)
{
	gen6_mfd_avc_phantom_slice(ctx, pic_param, next_slice_param, gen7_mfd_context->base.batch);
}

static void
gen8_mfd_avc_slice_state(VADriverContextP ctx,
						 VAPictureParameterBufferH264 *pic_param,
						 VASliceParameterBufferH264 *slice_param,
						 VASliceParameterBufferH264 *next_slice_param,
						 struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	int width_in_mbs = pic_param->picture_width_in_mbs_minus1 + 1;
	int height_in_mbs = pic_param->picture_height_in_mbs_minus1 + 1;
	int slice_hor_pos, slice_ver_pos, next_slice_hor_pos, next_slice_ver_pos;
	int num_ref_idx_l0, num_ref_idx_l1;
	int mbaff_picture = (!pic_param->pic_fields.bits.field_pic_flag &&
						 pic_param->seq_fields.bits.mb_adaptive_frame_field_flag);
	int first_mb_in_slice = 0, first_mb_in_next_slice = 0;
	int slice_type;
	int num_surfaces = 0;
	int i;

	if (slice_param->slice_type == SLICE_TYPE_I ||
		slice_param->slice_type == SLICE_TYPE_SI) {
		slice_type = SLICE_TYPE_I;
	} else if (slice_param->slice_type == SLICE_TYPE_P ||
			   slice_param->slice_type == SLICE_TYPE_SP) {
		slice_type = SLICE_TYPE_P;
	} else {
		assert(slice_param->slice_type == SLICE_TYPE_B);
		slice_type = SLICE_TYPE_B;
	}

	if (slice_type == SLICE_TYPE_I) {
		assert(slice_param->num_ref_idx_l0_active_minus1 == 0);
		assert(slice_param->num_ref_idx_l1_active_minus1 == 0);
		num_ref_idx_l0 = 0;
		num_ref_idx_l1 = 0;
	} else if (slice_type == SLICE_TYPE_P) {
		assert(slice_param->num_ref_idx_l1_active_minus1 == 0);
		num_ref_idx_l0 = slice_param->num_ref_idx_l0_active_minus1 + 1;
		num_ref_idx_l1 = 0;
	} else {
		num_ref_idx_l0 = slice_param->num_ref_idx_l0_active_minus1 + 1;
		num_ref_idx_l1 = slice_param->num_ref_idx_l1_active_minus1 + 1;
	}

	/* Don't bind a surface which doesn't exist, that crashes the GPU */
	for (i = 0; i < ARRAY_ELEMS(gen7_mfd_context->reference_surface); i++)
		if (gen7_mfd_context->reference_surface[i].surface_id != VA_INVALID_ID)
			num_surfaces ++;
	if (num_surfaces == 0) {
		num_ref_idx_l0 = 0;
		num_ref_idx_l1 = 0;
	}

	first_mb_in_slice = slice_param->first_mb_in_slice;
	slice_hor_pos = first_mb_in_slice % width_in_mbs;
	slice_ver_pos = first_mb_in_slice / width_in_mbs;

	if (mbaff_picture)
		slice_ver_pos = slice_ver_pos << 1;
	if (next_slice_param) {
		first_mb_in_next_slice = next_slice_param->first_mb_in_slice;
		next_slice_hor_pos = first_mb_in_next_slice % width_in_mbs;
		next_slice_ver_pos = first_mb_in_next_slice / width_in_mbs;

		if (mbaff_picture)
			next_slice_ver_pos = next_slice_ver_pos << 1;
	} else {
		next_slice_hor_pos = 0;
		next_slice_ver_pos = height_in_mbs / (1 + !!pic_param->pic_fields.bits.field_pic_flag);
	}

	BEGIN_BCS_BATCH(batch, 11); /* FIXME: is it 10??? */
	OUT_BCS_BATCH(batch, MFX_AVC_SLICE_STATE | (11 - 2));
	OUT_BCS_BATCH(batch, slice_type);
	OUT_BCS_BATCH(batch,
				  (num_ref_idx_l1 << 24) |
				  (num_ref_idx_l0 << 16) |
				  (slice_param->chroma_log2_weight_denom << 8) |
				  (slice_param->luma_log2_weight_denom << 0));
	OUT_BCS_BATCH(batch,
				  (slice_param->direct_spatial_mv_pred_flag << 29) |
				  (slice_param->disable_deblocking_filter_idc << 27) |
				  (slice_param->cabac_init_idc << 24) |
				  ((pic_param->pic_init_qp_minus26 + 26 + slice_param->slice_qp_delta) << 16) |
				  ((slice_param->slice_beta_offset_div2 & 0xf) << 8) |
				  ((slice_param->slice_alpha_c0_offset_div2 & 0xf) << 0));
	OUT_BCS_BATCH(batch,
				  (slice_ver_pos << 24) |
				  (slice_hor_pos << 16) |
				  (first_mb_in_slice << 0));
	OUT_BCS_BATCH(batch,
				  (next_slice_ver_pos << 16) |
				  (next_slice_hor_pos << 0));
	OUT_BCS_BATCH(batch,
				  (next_slice_param == NULL) << 19); /* last slice flag */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	ADVANCE_BCS_BATCH(batch);
}

static inline void
gen8_mfd_avc_ref_idx_state(VADriverContextP ctx,
						   VAPictureParameterBufferH264 *pic_param,
						   VASliceParameterBufferH264 *slice_param,
						   struct gen7_mfd_context *gen7_mfd_context)
{
	gen6_send_avc_ref_idx_state(
		gen7_mfd_context->base.batch,
		slice_param,
		gen7_mfd_context->reference_surface
	);
}

static void
gen8_mfd_avc_weightoffset_state(VADriverContextP ctx,
								VAPictureParameterBufferH264 *pic_param,
								VASliceParameterBufferH264 *slice_param,
								struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	int i, j, num_weight_offset_table = 0;
	short weightoffsets[32 * 6];

	if ((slice_param->slice_type == SLICE_TYPE_P ||
		 slice_param->slice_type == SLICE_TYPE_SP) &&
		(pic_param->pic_fields.bits.weighted_pred_flag == 1)) {
		num_weight_offset_table = 1;
	}

	if ((slice_param->slice_type == SLICE_TYPE_B) &&
		(pic_param->pic_fields.bits.weighted_bipred_idc == 1)) {
		num_weight_offset_table = 2;
	}

	for (i = 0; i < num_weight_offset_table; i++) {
		BEGIN_BCS_BATCH(batch, 98);
		OUT_BCS_BATCH(batch, MFX_AVC_WEIGHTOFFSET_STATE | (98 - 2));
		OUT_BCS_BATCH(batch, i);

		if (i == 0) {
			for (j = 0; j < 32; j++) {
				weightoffsets[j * 6 + 0] = slice_param->luma_weight_l0[j];
				weightoffsets[j * 6 + 1] = slice_param->luma_offset_l0[j];
				weightoffsets[j * 6 + 2] = slice_param->chroma_weight_l0[j][0];
				weightoffsets[j * 6 + 3] = slice_param->chroma_offset_l0[j][0];
				weightoffsets[j * 6 + 4] = slice_param->chroma_weight_l0[j][1];
				weightoffsets[j * 6 + 5] = slice_param->chroma_offset_l0[j][1];
			}
		} else {
			for (j = 0; j < 32; j++) {
				weightoffsets[j * 6 + 0] = slice_param->luma_weight_l1[j];
				weightoffsets[j * 6 + 1] = slice_param->luma_offset_l1[j];
				weightoffsets[j * 6 + 2] = slice_param->chroma_weight_l1[j][0];
				weightoffsets[j * 6 + 3] = slice_param->chroma_offset_l1[j][0];
				weightoffsets[j * 6 + 4] = slice_param->chroma_weight_l1[j][1];
				weightoffsets[j * 6 + 5] = slice_param->chroma_offset_l1[j][1];
			}
		}

		intel_batchbuffer_data(batch, weightoffsets, sizeof(weightoffsets));
		ADVANCE_BCS_BATCH(batch);
	}
}

static void
gen8_mfd_avc_bsd_object(VADriverContextP ctx,
						VAPictureParameterBufferH264 *pic_param,
						VASliceParameterBufferH264 *slice_param,
						dri_bo *slice_data_bo,
						VASliceParameterBufferH264 *next_slice_param,
						struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	unsigned int slice_data_bit_offset = avc_get_first_mb_bit_offset(slice_data_bo,
															slice_param,
															pic_param->pic_fields.bits.entropy_coding_mode_flag);

	unsigned int mb_byte_offset = slice_data_bit_offset;
	if (gen7_mfd_context->decoder_format_mode == MFX_SHORT_MODE)
	{
		/**
		 * IVB PRM, Vol 2, Part 3:
		 * "Short Format: it should be programmed to be 0. HW will parse the Slice Header."
		 */
		mb_byte_offset = 0;
	}							

	/* the input bitsteam format on GEN7 differs from GEN6 */
	BEGIN_BCS_BATCH(batch, 6);
	OUT_BCS_BATCH(batch, MFD_AVC_BSD_OBJECT | (6 - 2));
	OUT_BCS_BATCH(batch,
				  (slice_param->slice_data_size));
	OUT_BCS_BATCH(batch, slice_param->slice_data_offset);
	OUT_BCS_BATCH(batch,
				  (0 << 31) |
				  (0 << 14) |
				  (0 << 12) |
				  (0 << 10) |
				  (0 << 8));
	OUT_BCS_BATCH(batch,
				  ((mb_byte_offset >> 3) << 16) |
				  (1 << 7)  |
				  (0 << 5)  |
				  (0 << 4)  |
				  ((next_slice_param == NULL) << 3) | /* LastSlice Flag */
				  (slice_data_bit_offset & 0x7));
	OUT_BCS_BATCH(batch, 0);
	ADVANCE_BCS_BATCH(batch);
}

static inline void
gen8_mfd_avc_context_init(
	VADriverContextP         ctx,
	struct object_config *obj_config,
	struct gen7_mfd_context *gen7_mfd_context
)
{
	/* Initialize flat scaling lists */
	avc_gen_default_iq_matrix(&gen7_mfd_context->iq_matrix.h264);

	/* Setup up short format mode if requested. */
	VAConfigAttrib *attrib_found;
	attrib_found = gen7_lookup_config_attribute(obj_config, VAConfigAttribDecSliceMode);

	if (attrib_found && attrib_found->value == VA_DEC_SLICE_MODE_BASE)
	{
		gen7_mfd_context->decoder_format_mode = MFX_SHORT_MODE;
	}
}

static void
gen8_mfd_avc_decode_init(VADriverContextP ctx,
						 struct decode_state *decode_state,
						 struct gen7_mfd_context *gen7_mfd_context)
{
	VAPictureParameterBufferH264 *pic_param;
	VASliceParameterBufferH264 *slice_param;
	struct i965_driver_data *i965 = i965_driver_data(ctx);
	struct object_surface *obj_surface;
	dri_bo *bo;
	int i, j, enable_avc_ildb = 0;
	unsigned int width_in_mbs, height_in_mbs;

	for (j = 0; j < decode_state->num_slice_params && enable_avc_ildb == 0; j++) {
		assert(decode_state->slice_params && decode_state->slice_params[j]->buffer);
		slice_param = (VASliceParameterBufferH264 *)decode_state->slice_params[j]->buffer;

		for (i = 0; i < decode_state->slice_params[j]->num_elements; i++) {
			assert(slice_param->slice_data_flag == VA_SLICE_DATA_FLAG_ALL);
			assert((slice_param->slice_type == SLICE_TYPE_I) ||
				   (slice_param->slice_type == SLICE_TYPE_SI) ||
				   (slice_param->slice_type == SLICE_TYPE_P) ||
				   (slice_param->slice_type == SLICE_TYPE_SP) ||
				   (slice_param->slice_type == SLICE_TYPE_B));

			if (slice_param->disable_deblocking_filter_idc != 1) {
				enable_avc_ildb = 1;
				break;
			}

			slice_param++;
		}
	}

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferH264 *)decode_state->pic_param->buffer;
	gen75_update_avc_frame_store_index(ctx, decode_state, pic_param,
									   gen7_mfd_context->reference_surface);
	width_in_mbs = pic_param->picture_width_in_mbs_minus1 + 1;
	height_in_mbs = pic_param->picture_height_in_mbs_minus1 + 1;
	assert(width_in_mbs > 0 && width_in_mbs <= 256); /* 4K */
	assert(height_in_mbs > 0 && height_in_mbs <= 256);

	/* Current decoded picture */
	obj_surface = decode_state->render_object;
	if (pic_param->pic_fields.bits.reference_pic_flag)
		obj_surface->flags |= SURFACE_REFERENCED;
	else
		obj_surface->flags &= ~SURFACE_REFERENCED;

	avc_ensure_surface_bo(ctx, decode_state, obj_surface, pic_param);
	gen8_mfd_init_avc_surface(ctx, pic_param, obj_surface);

	dri_bo_unreference(gen7_mfd_context->post_deblocking_output.bo);
	gen7_mfd_context->post_deblocking_output.bo = obj_surface->bo;
	dri_bo_reference(gen7_mfd_context->post_deblocking_output.bo);
	gen7_mfd_context->post_deblocking_output.valid = enable_avc_ildb;

	dri_bo_unreference(gen7_mfd_context->pre_deblocking_output.bo);
	gen7_mfd_context->pre_deblocking_output.bo = obj_surface->bo;
	dri_bo_reference(gen7_mfd_context->pre_deblocking_output.bo);
	gen7_mfd_context->pre_deblocking_output.valid = !enable_avc_ildb;

	dri_bo_unreference(gen7_mfd_context->intra_row_store_scratch_buffer.bo);
	bo = dri_bo_alloc(i965->intel.bufmgr,
					  "intra row store",
					  width_in_mbs * 64,
					  0x1000);
	assert(bo);
	gen7_mfd_context->intra_row_store_scratch_buffer.bo = bo;
	gen7_mfd_context->intra_row_store_scratch_buffer.valid = 1;

	dri_bo_unreference(gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.bo);
	bo = dri_bo_alloc(i965->intel.bufmgr,
					  "deblocking filter row store",
					  width_in_mbs * 64 * 4,
					  0x1000);
	assert(bo);
	gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.bo = bo;
	gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.valid = 1;

	dri_bo_unreference(gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.bo);
	bo = dri_bo_alloc(i965->intel.bufmgr,
					  "bsd mpc row store",
					  width_in_mbs * 64 * 2,
					  0x1000);
	assert(bo);
	gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.bo = bo;
	gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.valid = 1;

	dri_bo_unreference(gen7_mfd_context->mpr_row_store_scratch_buffer.bo);
	bo = dri_bo_alloc(i965->intel.bufmgr,
					  "mpr row store",
					  width_in_mbs * 64 * 2,
					  0x1000);
	assert(bo);
	gen7_mfd_context->mpr_row_store_scratch_buffer.bo = bo;
	gen7_mfd_context->mpr_row_store_scratch_buffer.valid = 1;

	gen7_mfd_context->bitplane_read_buffer.valid = 0;
}

static void
gen8_mfd_avc_decode_picture(VADriverContextP ctx,
							struct decode_state *decode_state,
							struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	VAPictureParameterBufferH264 *pic_param;
	VASliceParameterBufferH264 *slice_param, *next_slice_param, *next_slice_group_param;
	dri_bo *slice_data_bo;
	int i, j;

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferH264 *)decode_state->pic_param->buffer;
	gen8_mfd_avc_decode_init(ctx, decode_state, gen7_mfd_context);

	intel_batchbuffer_start_atomic_bcs(batch, 0x1000);
	intel_batchbuffer_emit_mi_flush(batch);
	gen8_mfd_pipe_mode_select(ctx, decode_state, MFX_FORMAT_AVC, gen7_mfd_context);
	gen8_mfd_surface_state(ctx, decode_state, MFX_FORMAT_AVC, gen7_mfd_context);
	gen8_mfd_pipe_buf_addr_state(ctx, decode_state, MFX_FORMAT_AVC, gen7_mfd_context);
	gen8_mfd_bsp_buf_base_addr_state(ctx, decode_state, MFX_FORMAT_AVC, gen7_mfd_context);
	gen8_mfd_avc_qm_state(ctx, decode_state, gen7_mfd_context);
	gen8_mfd_avc_picid_state(ctx, decode_state, gen7_mfd_context);
	gen8_mfd_avc_img_state(ctx, decode_state, gen7_mfd_context);

	for (j = 0; j < decode_state->num_slice_params; j++) {
		assert(decode_state->slice_params && decode_state->slice_params[j]->buffer);
		slice_param = (VASliceParameterBufferH264 *)decode_state->slice_params[j]->buffer;
		slice_data_bo = decode_state->slice_datas[j]->bo;
		gen8_mfd_ind_obj_base_addr_state(ctx, slice_data_bo, MFX_FORMAT_AVC, gen7_mfd_context);

		if (j == decode_state->num_slice_params - 1)
			next_slice_group_param = NULL;
		else
			next_slice_group_param = (VASliceParameterBufferH264 *)decode_state->slice_params[j + 1]->buffer;

		if (j == 0 && slice_param->first_mb_in_slice)
			gen8_mfd_avc_phantom_slice_first(ctx, pic_param, slice_param, gen7_mfd_context);

		for (i = 0; i < decode_state->slice_params[j]->num_elements; i++) {
			assert(slice_param->slice_data_flag == VA_SLICE_DATA_FLAG_ALL);
			assert((slice_param->slice_type == SLICE_TYPE_I) ||
				   (slice_param->slice_type == SLICE_TYPE_SI) ||
				   (slice_param->slice_type == SLICE_TYPE_P) ||
				   (slice_param->slice_type == SLICE_TYPE_SP) ||
				   (slice_param->slice_type == SLICE_TYPE_B));

			if (i < decode_state->slice_params[j]->num_elements - 1)
				next_slice_param = slice_param + 1;
			else
				next_slice_param = next_slice_group_param;

			gen8_mfd_avc_directmode_state(ctx, decode_state, pic_param, slice_param, gen7_mfd_context);
			gen8_mfd_avc_ref_idx_state(ctx, pic_param, slice_param, gen7_mfd_context);
			gen8_mfd_avc_weightoffset_state(ctx, pic_param, slice_param, gen7_mfd_context);
			gen8_mfd_avc_slice_state(ctx, pic_param, slice_param, next_slice_param, gen7_mfd_context);
			gen8_mfd_avc_bsd_object(ctx, pic_param, slice_param, slice_data_bo, next_slice_param, gen7_mfd_context);
			slice_param++;
		}
	}

	intel_batchbuffer_end_atomic(batch);
	intel_batchbuffer_flush(batch);
}

static void
gen8_mfd_mpeg2_decode_init(VADriverContextP ctx,
						   struct decode_state *decode_state,
						   struct gen7_mfd_context *gen7_mfd_context)
{
	VAPictureParameterBufferMPEG2 *pic_param;
	struct i965_driver_data *i965 = i965_driver_data(ctx);
	struct object_surface *obj_surface;
	dri_bo *bo;
	unsigned int width_in_mbs;

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferMPEG2 *)decode_state->pic_param->buffer;
	width_in_mbs = ALIGN(pic_param->horizontal_size, 16) / 16;

	mpeg2_set_reference_surfaces(
		ctx,
		gen7_mfd_context->reference_surface,
		decode_state,
		pic_param
	);

	/* Current decoded picture */
	obj_surface = decode_state->render_object;
	i965_check_alloc_surface_bo(ctx, obj_surface, 1, VA_FOURCC_NV12, SUBSAMPLE_YUV420);

	dri_bo_unreference(gen7_mfd_context->pre_deblocking_output.bo);
	gen7_mfd_context->pre_deblocking_output.bo = obj_surface->bo;
	dri_bo_reference(gen7_mfd_context->pre_deblocking_output.bo);
	gen7_mfd_context->pre_deblocking_output.valid = 1;

	dri_bo_unreference(gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.bo);
	bo = dri_bo_alloc(i965->intel.bufmgr,
					  "bsd mpc row store",
					  width_in_mbs * 96,
					  0x1000);
	assert(bo);
	gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.bo = bo;
	gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.valid = 1;

	gen7_mfd_context->post_deblocking_output.valid = 0;
	gen7_mfd_context->intra_row_store_scratch_buffer.valid = 0;
	gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.valid = 0;
	gen7_mfd_context->mpr_row_store_scratch_buffer.valid = 0;
	gen7_mfd_context->bitplane_read_buffer.valid = 0;
}

static void
gen8_mfd_mpeg2_pic_state(VADriverContextP ctx,
						 struct decode_state *decode_state,
						 struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	VAPictureParameterBufferMPEG2 *pic_param;
	unsigned int slice_concealment_disable_bit = 0;

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferMPEG2 *)decode_state->pic_param->buffer;

	slice_concealment_disable_bit = 1;

	BEGIN_BCS_BATCH(batch, 13);
	OUT_BCS_BATCH(batch, MFX_MPEG2_PIC_STATE | (13 - 2));
	OUT_BCS_BATCH(batch,
				  (pic_param->f_code & 0xf) << 28 | /* f_code[1][1] */
				  ((pic_param->f_code >> 4) & 0xf) << 24 | /* f_code[1][0] */
				  ((pic_param->f_code >> 8) & 0xf) << 20 | /* f_code[0][1] */
				  ((pic_param->f_code >> 12) & 0xf) << 16 | /* f_code[0][0] */
				  pic_param->picture_coding_extension.bits.intra_dc_precision << 14 |
				  pic_param->picture_coding_extension.bits.picture_structure << 12 |
				  pic_param->picture_coding_extension.bits.top_field_first << 11 |
				  pic_param->picture_coding_extension.bits.frame_pred_frame_dct << 10 |
				  pic_param->picture_coding_extension.bits.concealment_motion_vectors << 9 |
				  pic_param->picture_coding_extension.bits.q_scale_type << 8 |
				  pic_param->picture_coding_extension.bits.intra_vlc_format << 7 |
				  pic_param->picture_coding_extension.bits.alternate_scan << 6);
	OUT_BCS_BATCH(batch,
				  pic_param->picture_coding_type << 9);
	OUT_BCS_BATCH(batch,
				  (slice_concealment_disable_bit << 31) |
				  ((ALIGN(pic_param->vertical_size, 16) / 16) - 1) << 16 |
				  ((ALIGN(pic_param->horizontal_size, 16) / 16) - 1));
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_mpeg2_qm_state(VADriverContextP ctx,
						struct decode_state *decode_state,
						struct gen7_mfd_context *gen7_mfd_context)
{
	VAIQMatrixBufferMPEG2 * const gen_iq_matrix = &gen7_mfd_context->iq_matrix.mpeg2;
	int i, j;

	/* Update internal QM state */
	if (decode_state->iq_matrix && decode_state->iq_matrix->buffer) {
		VAIQMatrixBufferMPEG2 * const iq_matrix =
			(VAIQMatrixBufferMPEG2 *)decode_state->iq_matrix->buffer;

		if (gen_iq_matrix->load_intra_quantiser_matrix == -1 ||
			iq_matrix->load_intra_quantiser_matrix) {
			gen_iq_matrix->load_intra_quantiser_matrix =
				iq_matrix->load_intra_quantiser_matrix;
			if (iq_matrix->load_intra_quantiser_matrix) {
				for (j = 0; j < 64; j++)
					gen_iq_matrix->intra_quantiser_matrix[zigzag_direct[j]] =
						iq_matrix->intra_quantiser_matrix[j];
			}
		}

		if (gen_iq_matrix->load_non_intra_quantiser_matrix == -1 ||
			iq_matrix->load_non_intra_quantiser_matrix) {
			gen_iq_matrix->load_non_intra_quantiser_matrix =
				iq_matrix->load_non_intra_quantiser_matrix;
			if (iq_matrix->load_non_intra_quantiser_matrix) {
				for (j = 0; j < 64; j++)
					gen_iq_matrix->non_intra_quantiser_matrix[zigzag_direct[j]] =
						iq_matrix->non_intra_quantiser_matrix[j];
			}
		}
	}

	/* Commit QM state to HW */
	for (i = 0; i < 2; i++) {
		unsigned char *qm = NULL;
		int qm_type;

		if (i == 0) {
			if (gen_iq_matrix->load_intra_quantiser_matrix) {
				qm = gen_iq_matrix->intra_quantiser_matrix;
				qm_type = MFX_QM_MPEG_INTRA_QUANTIZER_MATRIX;
			}
		} else {
			if (gen_iq_matrix->load_non_intra_quantiser_matrix) {
				qm = gen_iq_matrix->non_intra_quantiser_matrix;
				qm_type = MFX_QM_MPEG_NON_INTRA_QUANTIZER_MATRIX;
			}
		}

		if (!qm)
			continue;

		gen8_mfd_qm_state(ctx, qm_type, qm, 64, gen7_mfd_context);
	}
}

static void
gen8_mfd_mpeg2_bsd_object(VADriverContextP ctx,
						  VAPictureParameterBufferMPEG2 *pic_param,
						  VASliceParameterBufferMPEG2 *slice_param,
						  VASliceParameterBufferMPEG2 *next_slice_param,
						  struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	unsigned int width_in_mbs = ALIGN(pic_param->horizontal_size, 16) / 16;
	int mb_count, vpos0, hpos0, vpos1, hpos1, is_field_pic_wa, is_field_pic = 0;

	if (pic_param->picture_coding_extension.bits.picture_structure == MPEG_TOP_FIELD ||
		pic_param->picture_coding_extension.bits.picture_structure == MPEG_BOTTOM_FIELD)
		is_field_pic = 1;
	is_field_pic_wa = is_field_pic &&
					  gen7_mfd_context->wa_mpeg2_slice_vertical_position > 0;

	vpos0 = slice_param->slice_vertical_position / (1 + is_field_pic_wa);
	hpos0 = slice_param->slice_horizontal_position;

	if (next_slice_param == NULL) {
		vpos1 = ALIGN(pic_param->vertical_size, 16) / 16 / (1 + is_field_pic);
		hpos1 = 0;
	} else {
		vpos1 = next_slice_param->slice_vertical_position / (1 + is_field_pic_wa);
		hpos1 = next_slice_param->slice_horizontal_position;
	}

	mb_count = (vpos1 * width_in_mbs + hpos1) - (vpos0 * width_in_mbs + hpos0);

	BEGIN_BCS_BATCH(batch, 5);
	OUT_BCS_BATCH(batch, MFD_MPEG2_BSD_OBJECT | (5 - 2));
	OUT_BCS_BATCH(batch,
				  slice_param->slice_data_size - (slice_param->macroblock_offset >> 3));
	OUT_BCS_BATCH(batch,
				  slice_param->slice_data_offset + (slice_param->macroblock_offset >> 3));
	OUT_BCS_BATCH(batch,
				  hpos0 << 24 |
				  vpos0 << 16 |
				  mb_count << 8 |
				  (next_slice_param == NULL) << 5 |
				  (next_slice_param == NULL) << 3 |
				  (slice_param->macroblock_offset & 0x7));
	OUT_BCS_BATCH(batch,
				  (slice_param->quantiser_scale_code << 24) |
				  (vpos1 << 8 | hpos1));
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_mpeg2_decode_picture(VADriverContextP ctx,
							  struct decode_state *decode_state,
							  struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	VAPictureParameterBufferMPEG2 *pic_param;
	VASliceParameterBufferMPEG2 *slice_param, *next_slice_param, *next_slice_group_param;
	dri_bo *slice_data_bo;
	int i, j;

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferMPEG2 *)decode_state->pic_param->buffer;

	gen8_mfd_mpeg2_decode_init(ctx, decode_state, gen7_mfd_context);
	intel_batchbuffer_start_atomic_bcs(batch, 0x1000);
	intel_batchbuffer_emit_mi_flush(batch);
	gen8_mfd_pipe_mode_select(ctx, decode_state, MFX_FORMAT_MPEG2, gen7_mfd_context);
	gen8_mfd_surface_state(ctx, decode_state, MFX_FORMAT_MPEG2, gen7_mfd_context);
	gen8_mfd_pipe_buf_addr_state(ctx, decode_state, MFX_FORMAT_MPEG2, gen7_mfd_context);
	gen8_mfd_bsp_buf_base_addr_state(ctx, decode_state, MFX_FORMAT_MPEG2, gen7_mfd_context);
	gen8_mfd_mpeg2_pic_state(ctx, decode_state, gen7_mfd_context);
	gen8_mfd_mpeg2_qm_state(ctx, decode_state, gen7_mfd_context);

	if (gen7_mfd_context->wa_mpeg2_slice_vertical_position < 0)
		gen7_mfd_context->wa_mpeg2_slice_vertical_position =
			mpeg2_wa_slice_vertical_position(decode_state, pic_param);

	for (j = 0; j < decode_state->num_slice_params; j++) {
		assert(decode_state->slice_params && decode_state->slice_params[j]->buffer);
		slice_param = (VASliceParameterBufferMPEG2 *)decode_state->slice_params[j]->buffer;
		slice_data_bo = decode_state->slice_datas[j]->bo;
		gen8_mfd_ind_obj_base_addr_state(ctx, slice_data_bo, MFX_FORMAT_MPEG2, gen7_mfd_context);

		if (j == decode_state->num_slice_params - 1)
			next_slice_group_param = NULL;
		else
			next_slice_group_param = (VASliceParameterBufferMPEG2 *)decode_state->slice_params[j + 1]->buffer;

		for (i = 0; i < decode_state->slice_params[j]->num_elements; i++) {
			assert(slice_param->slice_data_flag == VA_SLICE_DATA_FLAG_ALL);

			if (i < decode_state->slice_params[j]->num_elements - 1)
				next_slice_param = slice_param + 1;
			else
				next_slice_param = next_slice_group_param;

			gen8_mfd_mpeg2_bsd_object(ctx, pic_param, slice_param, next_slice_param, gen7_mfd_context);
			slice_param++;
		}
	}

	intel_batchbuffer_end_atomic(batch);
	intel_batchbuffer_flush(batch);
}

static const int va_to_gen7_vc1_mv[4] = {
	1, /* 1-MV */
	2, /* 1-MV half-pel */
	3, /* 1-MV half-pef bilinear */
	0, /* Mixed MV */
};

static const int b_picture_scale_factor[21] = {
	128, 85,  170, 64,  192,
	51,  102, 153, 204, 43,
	215, 37,  74,  111, 148,
	185, 222, 32,  96,  160,
	224,
};

static const int va_to_gen7_vc1_condover[3] = {
	0,
	2,
	3
};

static const int fptype_to_picture_type[8][2] = {
	{GEN7_VC1_I_PICTURE, GEN7_VC1_I_PICTURE},
	{GEN7_VC1_I_PICTURE, GEN7_VC1_P_PICTURE},
	{GEN7_VC1_P_PICTURE, GEN7_VC1_I_PICTURE},
	{GEN7_VC1_P_PICTURE, GEN7_VC1_P_PICTURE},
	{GEN7_VC1_B_PICTURE, GEN7_VC1_B_PICTURE},
	{GEN7_VC1_B_PICTURE, GEN7_VC1_BI_PICTURE},
	{GEN7_VC1_BI_PICTURE, GEN7_VC1_B_PICTURE},
	{GEN7_VC1_BI_PICTURE, GEN7_VC1_BI_PICTURE}
};

static void
gen8_mfd_free_vc1_surface(void **data)
{
	struct gen7_vc1_surface *gen7_vc1_surface = *data;

	if (!gen7_vc1_surface)
		return;

	dri_bo_unreference(gen7_vc1_surface->dmv_top);
	dri_bo_unreference(gen7_vc1_surface->dmv_bottom);
	free(gen7_vc1_surface);
	*data = NULL;
}

static void
gen8_mfd_init_vc1_surface(VADriverContextP ctx,
						  VAPictureParameterBufferVC1 *pic_param,
						  struct object_surface *obj_surface)
{
	struct i965_driver_data *i965 = i965_driver_data(ctx);
	struct gen7_vc1_surface *gen7_vc1_surface = obj_surface->private_data;
	int height_in_mbs;
	int picture_type;
	int is_first_field = 1;

	if (!pic_param->sequence_fields.bits.interlace ||
		(pic_param->picture_fields.bits.frame_coding_mode < 2)) { /* Progressive or Frame-Interlace */
		picture_type = pic_param->picture_fields.bits.picture_type;
	} else {/* Field-Interlace */
		is_first_field = pic_param->picture_fields.bits.is_first_field;
		picture_type = fptype_to_picture_type[pic_param->picture_fields.bits.picture_type][!is_first_field];
	}

	obj_surface->free_private_data = gen8_mfd_free_vc1_surface;

	if (!gen7_vc1_surface) {
		gen7_vc1_surface = calloc(sizeof(struct gen7_vc1_surface), 1);

		if (!gen7_vc1_surface)
			return;

		assert((obj_surface->size & 0x3f) == 0);
		obj_surface->private_data = gen7_vc1_surface;
	}

	if (!pic_param->sequence_fields.bits.interlace ||
		pic_param->picture_fields.bits.frame_coding_mode < 2 || /* Progressive or Frame-Interlace */
		is_first_field) {
		gen7_vc1_surface->picture_type_top = 0;
		gen7_vc1_surface->picture_type_bottom = 0;
		gen7_vc1_surface->intensity_compensation_top = 0;
		gen7_vc1_surface->intensity_compensation_bottom = 0;
		gen7_vc1_surface->luma_scale_top[0] = 0;
		gen7_vc1_surface->luma_scale_top[1] = 0;
		gen7_vc1_surface->luma_scale_bottom[0] = 0;
		gen7_vc1_surface->luma_scale_bottom[1] = 0;
		gen7_vc1_surface->luma_shift_top[0] = 0;
		gen7_vc1_surface->luma_shift_top[1] = 0;
		gen7_vc1_surface->luma_shift_bottom[0] = 0;
		gen7_vc1_surface->luma_shift_bottom[1] = 0;
	}

	if (!pic_param->sequence_fields.bits.interlace ||
		pic_param->picture_fields.bits.frame_coding_mode < 2) { /* Progressive or Frame-Interlace */
		gen7_vc1_surface->picture_type_top = picture_type;
		gen7_vc1_surface->picture_type_bottom = picture_type;
	} else if (pic_param->picture_fields.bits.top_field_first ^ is_first_field)
		gen7_vc1_surface->picture_type_bottom = picture_type;
	else
		gen7_vc1_surface->picture_type_top = picture_type;

	/*
	 * The Direct MV buffer is scalable with frame height, but
	 * does not scale with frame width as the hardware assumes
	 * that frame width is fixed at 128 MBs.
	 */

	if (gen7_vc1_surface->dmv_top == NULL) {
		height_in_mbs = ALIGN(obj_surface->orig_height, 16) / 16;
		gen7_vc1_surface->dmv_top = dri_bo_alloc(i965->intel.bufmgr,
											 "direct mv w/r buffer",
											 128 * height_in_mbs * 64,
											 0x1000);
	}

	if (pic_param->sequence_fields.bits.interlace &&
		gen7_vc1_surface->dmv_bottom == NULL) {
		height_in_mbs = ALIGN(obj_surface->orig_height, 32) / 32;
		gen7_vc1_surface->dmv_bottom = dri_bo_alloc(i965->intel.bufmgr,
											 "direct mv w/r buffer",
											 128 * height_in_mbs * 64,
											 0x1000);
	}
}

static void
gen8_mfd_vc1_decode_init(VADriverContextP ctx,
						 struct decode_state *decode_state,
						 struct gen7_mfd_context *gen7_mfd_context)
{
	VAPictureParameterBufferVC1 *pic_param;
	struct i965_driver_data *i965 = i965_driver_data(ctx);
	struct object_surface *obj_surface;
	struct gen7_vc1_surface *gen7_vc1_current_surface;
	struct gen7_vc1_surface *gen7_vc1_forward_surface;
	dri_bo *bo;
	int width_in_mbs;
	int picture_type;
	int is_first_field = 1;
	int i;

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferVC1 *)decode_state->pic_param->buffer;
	width_in_mbs = ALIGN(pic_param->coded_width, 16) / 16;

	if (!pic_param->sequence_fields.bits.interlace ||
		(pic_param->picture_fields.bits.frame_coding_mode < 2)) { /* Progressive or Frame-Interlace */
		picture_type = pic_param->picture_fields.bits.picture_type;
	} else {/* Field-Interlace */
		is_first_field = pic_param->picture_fields.bits.is_first_field;
		picture_type = fptype_to_picture_type[pic_param->picture_fields.bits.picture_type][!is_first_field];
	}

	/* Current decoded picture */
	obj_surface = decode_state->render_object;
	i965_check_alloc_surface_bo(ctx, obj_surface, 1, VA_FOURCC_NV12, SUBSAMPLE_YUV420);
	gen8_mfd_init_vc1_surface(ctx, pic_param, obj_surface);

	dri_bo_unreference(gen7_mfd_context->post_deblocking_output.bo);
	gen7_mfd_context->post_deblocking_output.bo = obj_surface->bo;
	dri_bo_reference(gen7_mfd_context->post_deblocking_output.bo);

	dri_bo_unreference(gen7_mfd_context->pre_deblocking_output.bo);
	gen7_mfd_context->pre_deblocking_output.bo = obj_surface->bo;
	dri_bo_reference(gen7_mfd_context->pre_deblocking_output.bo);

	if (picture_type == GEN7_VC1_SKIPPED_PICTURE) {
		gen7_mfd_context->post_deblocking_output.valid = 0;
		gen7_mfd_context->pre_deblocking_output.valid = 1;
	} else {
		gen7_mfd_context->post_deblocking_output.valid = pic_param->entrypoint_fields.bits.loopfilter;
		gen7_mfd_context->pre_deblocking_output.valid = !pic_param->entrypoint_fields.bits.loopfilter;
	}

	intel_update_vc1_frame_store_index(ctx,
									   decode_state,
									   pic_param,
									   gen7_mfd_context->reference_surface);

	if (picture_type == GEN7_VC1_P_PICTURE) {
		obj_surface = decode_state->reference_objects[0];
		gen7_vc1_current_surface = (struct gen7_vc1_surface *)(decode_state->render_object->private_data);
		if (pic_param->forward_reference_picture != VA_INVALID_ID &&
			obj_surface)
			gen7_vc1_forward_surface = (struct gen7_vc1_surface *)(obj_surface->private_data);
		else
			gen7_vc1_forward_surface = NULL;

		if (!pic_param->sequence_fields.bits.interlace ||
			pic_param->picture_fields.bits.frame_coding_mode == 0) { /* Progressive */
			if (pic_param->mv_fields.bits.mv_mode == VAMvModeIntensityCompensation) {
				if (gen7_vc1_forward_surface) {
					gen7_vc1_forward_surface->intensity_compensation_top = 1;
					gen7_vc1_forward_surface->intensity_compensation_bottom = 1;
					gen7_vc1_forward_surface->luma_scale_top[0] = pic_param->luma_scale;
					gen7_vc1_forward_surface->luma_scale_bottom[0] = pic_param->luma_scale;
					gen7_vc1_forward_surface->luma_shift_top[0] = pic_param->luma_shift;
					gen7_vc1_forward_surface->luma_shift_bottom[0] = pic_param->luma_shift;
				}
			}
		} else if (pic_param->sequence_fields.bits.interlace &&
			pic_param->picture_fields.bits.frame_coding_mode == 1) { /* Frame-Interlace */
			if (pic_param->picture_fields.bits.intensity_compensation) {
				if (gen7_vc1_forward_surface) {
					gen7_vc1_forward_surface->intensity_compensation_top = 1;
					gen7_vc1_forward_surface->intensity_compensation_bottom = 1;
					gen7_vc1_forward_surface->luma_scale_top[0] = pic_param->luma_scale;
					gen7_vc1_forward_surface->luma_scale_bottom[0] = pic_param->luma_scale;
					gen7_vc1_forward_surface->luma_shift_top[0] = pic_param->luma_shift;
					gen7_vc1_forward_surface->luma_shift_bottom[0] = pic_param->luma_shift;
				}
			}
		} else if (pic_param->sequence_fields.bits.interlace &&
				   pic_param->picture_fields.bits.frame_coding_mode == 2) { /* Field-Interlace */
			if (pic_param->mv_fields.bits.mv_mode == VAMvModeIntensityCompensation) {
				if (pic_param->intensity_compensation_field == 1 || /* Top field */
					pic_param->intensity_compensation_field == 0) { /* Both fields */
					if (is_first_field) {
						if ((!pic_param->reference_fields.bits.num_reference_pictures &&
							 (pic_param->reference_fields.bits.reference_field_pic_indicator ==
							 pic_param->picture_fields.bits.top_field_first)) ||
							pic_param->reference_fields.bits.num_reference_pictures) {
							if (gen7_vc1_forward_surface) {
								i = gen7_vc1_forward_surface->intensity_compensation_top++;
								gen7_vc1_forward_surface->luma_scale_top[i] = pic_param->luma_scale;
								gen7_vc1_forward_surface->luma_shift_top[i] = pic_param->luma_shift;
							}
						}
					} else { /* Second field */
						if (pic_param->picture_fields.bits.top_field_first) {
							if ((!pic_param->reference_fields.bits.num_reference_pictures &&
								 !pic_param->reference_fields.bits.reference_field_pic_indicator) ||
								pic_param->reference_fields.bits.num_reference_pictures) {
								i = gen7_vc1_current_surface->intensity_compensation_top++;
								gen7_vc1_current_surface->luma_scale_top[i] = pic_param->luma_scale;
								gen7_vc1_current_surface->luma_shift_top[i] = pic_param->luma_shift;
							}
						} else {
							if ((!pic_param->reference_fields.bits.num_reference_pictures &&
								 pic_param->reference_fields.bits.reference_field_pic_indicator) ||
								pic_param->reference_fields.bits.num_reference_pictures) {
								if (gen7_vc1_forward_surface) {
									i = gen7_vc1_forward_surface->intensity_compensation_top++;
									gen7_vc1_forward_surface->luma_scale_top[i] = pic_param->luma_scale;
									gen7_vc1_forward_surface->luma_shift_top[i] = pic_param->luma_shift;
								}
							}
						}
					}
				}
				if (pic_param->intensity_compensation_field == 2 || /* Bottom field */
					pic_param->intensity_compensation_field == 0) { /* Both fields */
					if (is_first_field) {
						if ((!pic_param->reference_fields.bits.num_reference_pictures &&
							 (pic_param->reference_fields.bits.reference_field_pic_indicator ^
							  pic_param->picture_fields.bits.top_field_first)) ||
							pic_param->reference_fields.bits.num_reference_pictures) {
							if (gen7_vc1_forward_surface) {
								i = gen7_vc1_forward_surface->intensity_compensation_bottom++;
								if (pic_param->intensity_compensation_field == 2) { /* Bottom field */
									gen7_vc1_forward_surface->luma_scale_bottom[i] = pic_param->luma_scale;
									gen7_vc1_forward_surface->luma_shift_bottom[i] = pic_param->luma_shift;
								} else { /* Both fields */
									gen7_vc1_forward_surface->luma_scale_bottom[i] = pic_param->luma_scale2;
									gen7_vc1_forward_surface->luma_shift_bottom[i] = pic_param->luma_shift2;
								}
							}
						}
					} else { /* Second field */
						if (pic_param->picture_fields.bits.top_field_first) {
							if ((!pic_param->reference_fields.bits.num_reference_pictures &&
								 pic_param->reference_fields.bits.reference_field_pic_indicator) ||
								pic_param->reference_fields.bits.num_reference_pictures) {
								if (gen7_vc1_forward_surface) {
									i = gen7_vc1_forward_surface->intensity_compensation_bottom++;
									if (pic_param->intensity_compensation_field == 2) { /* Bottom field */
										gen7_vc1_forward_surface->luma_scale_bottom[i] = pic_param->luma_scale;
										gen7_vc1_forward_surface->luma_shift_bottom[i] = pic_param->luma_shift;
									} else { /* Both fields */
										gen7_vc1_forward_surface->luma_scale_bottom[i] = pic_param->luma_scale2;
										gen7_vc1_forward_surface->luma_shift_bottom[i] = pic_param->luma_shift2;
									}
								}
							}
						} else {
						   if ((!pic_param->reference_fields.bits.num_reference_pictures &&
								 !pic_param->reference_fields.bits.reference_field_pic_indicator) ||
								pic_param->reference_fields.bits.num_reference_pictures) {
								i = gen7_vc1_current_surface->intensity_compensation_bottom++;
							   if (pic_param->intensity_compensation_field == 2) { /* Bottom field */
								   gen7_vc1_current_surface->luma_scale_bottom[i] = pic_param->luma_scale;
								   gen7_vc1_current_surface->luma_shift_bottom[i] = pic_param->luma_shift;
								} else { /* Both fields */
									gen7_vc1_current_surface->luma_scale_bottom[i] = pic_param->luma_scale2;
									gen7_vc1_current_surface->luma_shift_bottom[i] = pic_param->luma_shift2;
								}
							}
						}
					}
				}
			}
		}
	}

	dri_bo_unreference(gen7_mfd_context->intra_row_store_scratch_buffer.bo);
	bo = dri_bo_alloc(i965->intel.bufmgr,
					  "intra row store",
					  width_in_mbs * 64,
					  0x1000);
	assert(bo);
	gen7_mfd_context->intra_row_store_scratch_buffer.bo = bo;
	gen7_mfd_context->intra_row_store_scratch_buffer.valid = 1;

	dri_bo_unreference(gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.bo);
	bo = dri_bo_alloc(i965->intel.bufmgr,
					  "deblocking filter row store",
					  width_in_mbs * 7 * 64,
					  0x1000);
	assert(bo);
	gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.bo = bo;
	gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.valid = 1;

	dri_bo_unreference(gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.bo);
	bo = dri_bo_alloc(i965->intel.bufmgr,
					  "bsd mpc row store",
					  width_in_mbs * 96,
					  0x1000);
	assert(bo);
	gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.bo = bo;
	gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.valid = 1;

	gen7_mfd_context->mpr_row_store_scratch_buffer.valid = 0;

	if (picture_type == GEN7_VC1_SKIPPED_PICTURE)
		gen7_mfd_context->bitplane_read_buffer.valid = 1;
	else
		gen7_mfd_context->bitplane_read_buffer.valid = !!(pic_param->bitplane_present.value & 0x7f);
	dri_bo_unreference(gen7_mfd_context->bitplane_read_buffer.bo);

	if (gen7_mfd_context->bitplane_read_buffer.valid) {
		int width_in_mbs = ALIGN(pic_param->coded_width, 16) / 16;
		int height_in_mbs;
		int bitplane_width = ALIGN(width_in_mbs, 2) / 2;
		int src_w, src_h;
		uint8_t *src = NULL, *dst = NULL;

		if (!pic_param->sequence_fields.bits.interlace ||
			(pic_param->picture_fields.bits.frame_coding_mode < 2)) /* Progressive or Frame-Interlace */
			height_in_mbs = ALIGN(pic_param->coded_height, 16) / 16;
		else /* Field-Interlace */
			height_in_mbs = ALIGN(pic_param->coded_height, 32) / 32;

		bo = dri_bo_alloc(i965->intel.bufmgr,
						  "VC-1 Bitplane",
						  bitplane_width * height_in_mbs,
						  0x1000);
		assert(bo);
		gen7_mfd_context->bitplane_read_buffer.bo = bo;

		dri_bo_map(bo, True);
		assert(bo->virtual);
		dst = bo->virtual;

		if (picture_type == GEN7_VC1_SKIPPED_PICTURE) {
			for (src_h = 0; src_h < height_in_mbs; src_h++) {
				for (src_w = 0; src_w < width_in_mbs; src_w++) {
					int dst_index;
					uint8_t src_value = 0x2;

					dst_index = src_w / 2;
					dst[dst_index] = ((dst[dst_index] >> 4) | (src_value << 4));
				}

				if (src_w & 1)
					dst[src_w / 2] >>= 4;

				dst += bitplane_width;
			}
		} else {
			assert(decode_state->bit_plane->buffer);
			src = decode_state->bit_plane->buffer;

			for (src_h = 0; src_h < height_in_mbs; src_h++) {
				for (src_w = 0; src_w < width_in_mbs; src_w++) {
					int src_index, dst_index;
					int src_shift;
					uint8_t src_value;

					src_index = (src_h * width_in_mbs + src_w) / 2;
					src_shift = !((src_h * width_in_mbs + src_w) & 1) * 4;
					src_value = ((src[src_index] >> src_shift) & 0xf);

					dst_index = src_w / 2;
					dst[dst_index] = ((dst[dst_index] >> 4) | (src_value << 4));
				}

				if (src_w & 1)
					dst[src_w / 2] >>= 4;

				dst += bitplane_width;
			}
		}

		dri_bo_unmap(bo);
	} else
		gen7_mfd_context->bitplane_read_buffer.bo = NULL;
}

static void
gen8_mfd_vc1_pic_state(VADriverContextP ctx,
					   struct decode_state *decode_state,
					   struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	VAPictureParameterBufferVC1 *pic_param;
	struct object_surface *obj_surface;
	struct gen7_vc1_surface *gen7_vc1_current_surface;
	struct gen7_vc1_surface *gen7_vc1_reference_surface;
	int alt_pquant_config = 0, alt_pquant_edge_mask = 0, alt_pq;
	int dquant, dquantfrm, dqprofile, dqdbedge, dqsbedge, dqbilevel;
	int unified_mv_mode = 0;
	int ref_field_pic_polarity = 0;
	int scale_factor = 0;
	int trans_ac_y = 0;
	int dmv_surface_valid = 0;
	int frfd = 0;
	int brfd = 0;
	int fcm = 0;
	int picture_type;
	int ptype;
	int overlap = 0;
	int interpolation_mode = 0;
	int height_in_mbs;
	int is_first_field = 1;
	int loopfilter = 0;
	int bitplane_present;
	int range_reduction = 0;
	int range_reduction_scale = 0;
	int forward_mb = 0, mv_type_mb = 0, skip_mb = 0, direct_mb = 0;
	int overflags = 0, ac_pred = 0, field_tx = 0;

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferVC1 *)decode_state->pic_param->buffer;
	gen7_vc1_current_surface = (struct gen7_vc1_surface *)(decode_state->render_object->private_data);

	if (!pic_param->sequence_fields.bits.interlace ||
		(pic_param->picture_fields.bits.frame_coding_mode < 2)) { /* Progressive or Frame-Interlace */
		picture_type = pic_param->picture_fields.bits.picture_type;
		height_in_mbs = ALIGN(pic_param->coded_height, 16) / 16;
	} else {/* Field-Interlace */
		is_first_field = pic_param->picture_fields.bits.is_first_field;
		picture_type = fptype_to_picture_type[pic_param->picture_fields.bits.picture_type][!is_first_field];
		height_in_mbs = ALIGN(pic_param->coded_height, 32) / 32;
	}

	dquant = pic_param->pic_quantizer_fields.bits.dquant;
	dquantfrm = pic_param->pic_quantizer_fields.bits.dq_frame;
	dqprofile = pic_param->pic_quantizer_fields.bits.dq_profile;
	dqdbedge = pic_param->pic_quantizer_fields.bits.dq_db_edge;
	dqsbedge = pic_param->pic_quantizer_fields.bits.dq_sb_edge;
	dqbilevel = pic_param->pic_quantizer_fields.bits.dq_binary_level;
	alt_pq = pic_param->pic_quantizer_fields.bits.alt_pic_quantizer;

	if (dquant == 0) {
		alt_pquant_config = 0;
		alt_pquant_edge_mask = 0;
	} else if (dquant == 2) {
		alt_pquant_config = 1;
		alt_pquant_edge_mask = 0xf;
	} else {
		assert(dquant == 1);
		if (dquantfrm == 0) {
			alt_pquant_config = 0;
			alt_pquant_edge_mask = 0;
			alt_pq = 0;
		} else {
			assert(dquantfrm == 1);
			alt_pquant_config = 1;

			switch (dqprofile) {
			case 3:
				if (dqbilevel == 0) {
					alt_pquant_config = 2;
					alt_pquant_edge_mask = 0;
				} else {
					assert(dqbilevel == 1);
					alt_pquant_config = 3;
					alt_pquant_edge_mask = 0;
				}
				break;

			case 0:
				alt_pquant_edge_mask = 0xf;
				break;

			case 1:
				if (dqdbedge == 3)
					alt_pquant_edge_mask = 0x9;
				else
					alt_pquant_edge_mask = (0x3 << dqdbedge);

				break;

			case 2:
				alt_pquant_edge_mask = (0x1 << dqsbedge);
				break;

			default:
				assert(0);
			}
		}
	}

	if (pic_param->sequence_fields.bits.profile == 1 && /* Main Profile */
		pic_param->sequence_fields.bits.rangered) {
		obj_surface = decode_state->reference_objects[0];

		if (pic_param->forward_reference_picture != VA_INVALID_ID &&
			obj_surface)
			gen7_vc1_reference_surface = (struct gen7_vc1_surface *)(obj_surface->private_data);
		else
			gen7_vc1_reference_surface = NULL;

		if (picture_type == GEN7_VC1_SKIPPED_PICTURE)
			if (gen7_vc1_reference_surface)
				gen7_vc1_current_surface->range_reduction_frame = gen7_vc1_reference_surface->range_reduction_frame;
			else
				gen7_vc1_current_surface->range_reduction_frame = 0;
		else
			gen7_vc1_current_surface->range_reduction_frame = pic_param->range_reduction_frame;

		if (gen7_vc1_reference_surface) {
			if (gen7_vc1_current_surface->range_reduction_frame &&
				!gen7_vc1_reference_surface->range_reduction_frame) {
				range_reduction = 1;
				range_reduction_scale = 0;
			} else if (!gen7_vc1_current_surface->range_reduction_frame &&
					   gen7_vc1_reference_surface->range_reduction_frame) {
				range_reduction = 1;
				range_reduction_scale = 1;
			}
		}
	}

	if ((!pic_param->sequence_fields.bits.interlace ||
		 pic_param->picture_fields.bits.frame_coding_mode != 1) && /* Progressive or Field-Interlace */
		(picture_type == GEN7_VC1_P_PICTURE ||
		 picture_type == GEN7_VC1_B_PICTURE)) {
		if (pic_param->mv_fields.bits.mv_mode == VAMvModeIntensityCompensation) {
			assert(pic_param->mv_fields.bits.mv_mode2 < 4);
			unified_mv_mode = va_to_gen7_vc1_mv[pic_param->mv_fields.bits.mv_mode2];
		} else {
			assert(pic_param->mv_fields.bits.mv_mode < 4);
			unified_mv_mode = va_to_gen7_vc1_mv[pic_param->mv_fields.bits.mv_mode];
		}
	}

	if (pic_param->sequence_fields.bits.interlace &&
		pic_param->picture_fields.bits.frame_coding_mode == 2 && /* Field-Interlace */
		picture_type == GEN7_VC1_P_PICTURE &&
		!pic_param->reference_fields.bits.num_reference_pictures) {
		if (pic_param->reference_fields.bits.reference_field_pic_indicator == 0) {
			ref_field_pic_polarity = is_first_field ?
										pic_param->picture_fields.bits.top_field_first :
										!pic_param->picture_fields.bits.top_field_first;
		} else {
			ref_field_pic_polarity = is_first_field ?
										!pic_param->picture_fields.bits.top_field_first :
										pic_param->picture_fields.bits.top_field_first;
		}
	}

	if (pic_param->b_picture_fraction < 21)
		scale_factor = b_picture_scale_factor[pic_param->b_picture_fraction];

	if (picture_type == GEN7_VC1_SKIPPED_PICTURE) {
		ptype = GEN7_VC1_P_PICTURE;
		bitplane_present = 1;
	} else {
		ptype = pic_param->picture_fields.bits.picture_type;
		bitplane_present = !!(pic_param->bitplane_present.value & 0x7f);
		forward_mb = pic_param->raw_coding.flags.forward_mb;
		mv_type_mb = pic_param->raw_coding.flags.mv_type_mb;
		skip_mb = pic_param->raw_coding.flags.skip_mb;
		direct_mb = pic_param->raw_coding.flags.direct_mb;
		overflags = pic_param->raw_coding.flags.overflags;
		ac_pred = pic_param->raw_coding.flags.ac_pred;
		field_tx = pic_param->raw_coding.flags.field_tx;
		loopfilter = pic_param->entrypoint_fields.bits.loopfilter;
	}

	if (picture_type == GEN7_VC1_I_PICTURE || picture_type == GEN7_VC1_BI_PICTURE) /* I picture */
		trans_ac_y = pic_param->transform_fields.bits.transform_ac_codingset_idx2;
	else {
		trans_ac_y = pic_param->transform_fields.bits.transform_ac_codingset_idx1;

		/*
		 * 8.3.6.2.1 Transform Type Selection
		 * If variable-sized transform coding is not enabled,
		 * then the 8x8 transform shall be used for all blocks.
		 * it is also MFX_VC1_PIC_STATE requirement.
		 */
		if (pic_param->transform_fields.bits.variable_sized_transform_flag == 0) {
			pic_param->transform_fields.bits.mb_level_transform_type_flag   = 1;
			pic_param->transform_fields.bits.frame_level_transform_type     = 0;
		}
	}

	if (picture_type == GEN7_VC1_B_PICTURE) {
		obj_surface = decode_state->reference_objects[1];

		if (pic_param->backward_reference_picture != VA_INVALID_ID &&
			obj_surface)
			gen7_vc1_reference_surface = (struct gen7_vc1_surface *)(obj_surface->private_data);
		else
			gen7_vc1_reference_surface = NULL;

		if (gen7_vc1_reference_surface) {
			if (pic_param->sequence_fields.bits.interlace &&
				pic_param->picture_fields.bits.frame_coding_mode == 2 && /* Field-Interlace */
				pic_param->picture_fields.bits.top_field_first ^ is_first_field) {
				if (gen7_vc1_reference_surface->picture_type_bottom == GEN7_VC1_P_PICTURE)
					dmv_surface_valid = 1;
			} else if (gen7_vc1_reference_surface->picture_type_top == GEN7_VC1_P_PICTURE)
				dmv_surface_valid = 1;
		}
	}

	assert(pic_param->picture_fields.bits.frame_coding_mode < 3);

	gen7_vc1_current_surface->frame_coding_mode = pic_param->picture_fields.bits.frame_coding_mode;
	if (pic_param->sequence_fields.bits.interlace) {
		if (pic_param->picture_fields.bits.frame_coding_mode < 2)
			fcm = pic_param->picture_fields.bits.frame_coding_mode;
		else if (!pic_param->picture_fields.bits.top_field_first)
			fcm = 3; /* Field with bottom field first */
		else
			fcm = 2; /* Field with top field first */
	}

	if (pic_param->sequence_fields.bits.interlace &&
		pic_param->picture_fields.bits.frame_coding_mode == 2) { /* Field-Interlace */
		if (picture_type == GEN7_VC1_I_PICTURE ||
			 picture_type == GEN7_VC1_P_PICTURE) {

			if (is_first_field)
				gen7_vc1_current_surface->reference_distance = pic_param->reference_fields.bits.reference_distance;

			frfd = gen7_vc1_current_surface->reference_distance;
		} else if (picture_type == GEN7_VC1_B_PICTURE) {
			obj_surface = decode_state->reference_objects[1];

			if (pic_param->backward_reference_picture != VA_INVALID_ID &&
				obj_surface)
				gen7_vc1_reference_surface = (struct gen7_vc1_surface *)(obj_surface->private_data);
			else
				gen7_vc1_reference_surface = NULL;

			if (gen7_vc1_reference_surface) {
				frfd = (scale_factor * gen7_vc1_reference_surface->reference_distance) >> 8;

				brfd = gen7_vc1_reference_surface->reference_distance - frfd - 1;
				if (brfd < 0)
					brfd = 0;
			}
		}
	}

	if (pic_param->sequence_fields.bits.overlap) {
		if (pic_param->sequence_fields.bits.profile == 3) { /* Advanced Profile */
			if (picture_type == GEN7_VC1_P_PICTURE &&
				pic_param->pic_quantizer_fields.bits.pic_quantizer_scale >= 9) {
				overlap = 1;
			}
			if (picture_type == GEN7_VC1_I_PICTURE ||
				picture_type == GEN7_VC1_BI_PICTURE) {
				if (pic_param->pic_quantizer_fields.bits.pic_quantizer_scale >= 9) {
					overlap = 1;
				} else if (pic_param->conditional_overlap_flag == 1 || /* all block boundaries */
						   pic_param->conditional_overlap_flag == 2) { /* coded by OVERFLAGSMB bitplane */
					overlap = 1;
				}
			}
		} else {
			if (pic_param->pic_quantizer_fields.bits.pic_quantizer_scale >= 9 &&
				picture_type != GEN7_VC1_B_PICTURE) {
				overlap = 1;
			}
		}
	}

	if ((!pic_param->sequence_fields.bits.interlace ||
		 pic_param->picture_fields.bits.frame_coding_mode != 1) && /* Progressive or Field-Interlace */
		(picture_type == GEN7_VC1_P_PICTURE ||
		 picture_type == GEN7_VC1_B_PICTURE)) {
		if (pic_param->mv_fields.bits.mv_mode == VAMvMode1MvHalfPelBilinear ||
			(pic_param->mv_fields.bits.mv_mode == VAMvModeIntensityCompensation &&
			 pic_param->mv_fields.bits.mv_mode2 == VAMvMode1MvHalfPelBilinear))
			interpolation_mode = 8 | pic_param->fast_uvmc_flag;
		else
			interpolation_mode = 0 | pic_param->fast_uvmc_flag;
	}

	BEGIN_BCS_BATCH(batch, 6);
	OUT_BCS_BATCH(batch, MFD_VC1_LONG_PIC_STATE | (6 - 2));
	OUT_BCS_BATCH(batch,
				  ((height_in_mbs - 1) << 16) |
				  ((ALIGN(pic_param->coded_width, 16) / 16) - 1));
	OUT_BCS_BATCH(batch,
				  ((ALIGN(pic_param->coded_width, 16) / 16 + 1) / 2 - 1) << 24 |
				  dmv_surface_valid << 15 |
				  (pic_param->pic_quantizer_fields.bits.quantizer == 0) << 14 | /* implicit quantizer */
				  pic_param->rounding_control << 13 |
				  pic_param->sequence_fields.bits.syncmarker << 12 |
				  interpolation_mode << 8 |
				  range_reduction_scale << 7 |
				  range_reduction << 6 |
				  loopfilter << 5 |
				  overlap << 4 |
				  !is_first_field << 3 |
				  (pic_param->sequence_fields.bits.profile == 3) << 0); /* Advanced Profile */
	OUT_BCS_BATCH(batch,
				  va_to_gen7_vc1_condover[pic_param->conditional_overlap_flag] << 29 |
				  ptype << 26 |
				  fcm << 24 |
				  alt_pq << 16 |
				  pic_param->pic_quantizer_fields.bits.pic_quantizer_scale << 8 |
				  scale_factor << 0);
	OUT_BCS_BATCH(batch,
				  unified_mv_mode << 28 |
				  pic_param->mv_fields.bits.four_mv_switch << 27 |
				  pic_param->fast_uvmc_flag << 26 |
				  ref_field_pic_polarity << 25 |
				  pic_param->reference_fields.bits.num_reference_pictures << 24 |
				  brfd << 20 |
				  frfd << 16 |
				  pic_param->mv_fields.bits.extended_dmv_range << 10 |
				  pic_param->mv_fields.bits.extended_mv_range << 8 |
				  alt_pquant_edge_mask << 4 |
				  alt_pquant_config << 2 |
				  pic_param->pic_quantizer_fields.bits.half_qp << 1 |
				  pic_param->pic_quantizer_fields.bits.pic_quantizer_type << 0);
	OUT_BCS_BATCH(batch,
				  bitplane_present << 31 |
				  forward_mb << 30 |
				  mv_type_mb << 29 |
				  skip_mb << 28 |
				  direct_mb << 27 |
				  overflags << 26 |
				  ac_pred << 25 |
				  field_tx << 24 |
				  pic_param->mv_fields.bits.mv_table << 20 |
				  pic_param->mv_fields.bits.four_mv_block_pattern_table << 18 |
				  pic_param->mv_fields.bits.two_mv_block_pattern_table << 16 |
				  pic_param->transform_fields.bits.frame_level_transform_type << 12 |
				  pic_param->transform_fields.bits.mb_level_transform_type_flag << 11 |
				  pic_param->mb_mode_table << 8 |
				  trans_ac_y << 6 |
				  pic_param->transform_fields.bits.transform_ac_codingset_idx1 << 4 |
				  pic_param->transform_fields.bits.intra_transform_dc_table << 3 |
				  pic_param->cbp_table << 0);
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_vc1_pred_pipe_state(VADriverContextP ctx,
							 struct decode_state *decode_state,
							 struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	VAPictureParameterBufferVC1 *pic_param;
	struct gen7_vc1_surface *gen7_vc1_top_surface;
	struct gen7_vc1_surface *gen7_vc1_bottom_surface;
	int picture_type;
	int is_first_field = 1;
	int intensitycomp_single_fwd = 0;
	int intensitycomp_single_bwd = 0;
	int intensitycomp_double_fwd = 0;
	int lumscale1_single_fwd = 0;
	int lumscale2_single_fwd = 0;
	int lumshift1_single_fwd = 0;
	int lumshift2_single_fwd = 0;
	int lumscale1_single_bwd = 0;
	int lumscale2_single_bwd = 0;
	int lumshift1_single_bwd = 0;
	int lumshift2_single_bwd = 0;
	int lumscale1_double_fwd = 0;
	int lumscale2_double_fwd = 0;
	int lumshift1_double_fwd = 0;
	int lumshift2_double_fwd = 0;
	int replication_mode = 0;

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferVC1 *)decode_state->pic_param->buffer;

	if (!pic_param->sequence_fields.bits.interlace ||
		(pic_param->picture_fields.bits.frame_coding_mode < 2)) { /* Progressive or Frame-Interlace */
		picture_type = pic_param->picture_fields.bits.picture_type;
	} else {/* Field-Interlace */
		is_first_field = pic_param->picture_fields.bits.is_first_field;
		picture_type = fptype_to_picture_type[pic_param->picture_fields.bits.picture_type][!is_first_field];
	}

	if (picture_type == GEN7_VC1_P_PICTURE ||
		picture_type == GEN7_VC1_B_PICTURE) {
		if (gen7_mfd_context->reference_surface[0].surface_id != VA_INVALID_ID)
			gen7_vc1_top_surface = (struct gen7_vc1_surface *)(gen7_mfd_context->reference_surface[0].obj_surface->private_data);
		else
			gen7_vc1_top_surface = NULL;

		if (gen7_vc1_top_surface) {
			intensitycomp_single_fwd = !!gen7_vc1_top_surface->intensity_compensation_top;
			lumscale1_single_fwd = gen7_vc1_top_surface->luma_scale_top[0];
			lumshift1_single_fwd = gen7_vc1_top_surface->luma_shift_top[0];
			if (gen7_vc1_top_surface->intensity_compensation_top == 2) {
				intensitycomp_double_fwd = 1;
				lumscale1_double_fwd = gen7_vc1_top_surface->luma_scale_top[1];
				lumshift1_double_fwd = gen7_vc1_top_surface->luma_shift_top[1];
			}
			replication_mode |= !!gen7_vc1_top_surface->frame_coding_mode;
		}

		if (pic_param->sequence_fields.bits.interlace &&
			pic_param->picture_fields.bits.frame_coding_mode > 0) { /* Frame-Interlace or Field-Interlace */
			if (gen7_mfd_context->reference_surface[2].surface_id != VA_INVALID_ID)
				gen7_vc1_bottom_surface = (struct gen7_vc1_surface *)(gen7_mfd_context->reference_surface[2].obj_surface->private_data);
			else
				gen7_vc1_bottom_surface = NULL;

			if (gen7_vc1_bottom_surface) {
				intensitycomp_single_fwd |= !!gen7_vc1_bottom_surface->intensity_compensation_bottom << 1;
				lumscale2_single_fwd = gen7_vc1_bottom_surface->luma_scale_bottom[0];
				lumshift2_single_fwd = gen7_vc1_bottom_surface->luma_shift_bottom[0];
				if (gen7_vc1_bottom_surface->intensity_compensation_bottom == 2) {
					intensitycomp_double_fwd |= 2;
					lumscale2_double_fwd = gen7_vc1_bottom_surface->luma_scale_bottom[1];
					lumshift2_double_fwd = gen7_vc1_bottom_surface->luma_shift_bottom[1];
				}
				replication_mode |= !!gen7_vc1_bottom_surface->frame_coding_mode << 2;
			}
		}
	}

	if (picture_type == GEN7_VC1_B_PICTURE) {
		if (gen7_mfd_context->reference_surface[1].surface_id != VA_INVALID_ID)
			gen7_vc1_top_surface = (struct gen7_vc1_surface *)(gen7_mfd_context->reference_surface[1].obj_surface->private_data);
		else
			gen7_vc1_top_surface = NULL;

		if (gen7_vc1_top_surface) {
			intensitycomp_single_bwd = !!gen7_vc1_top_surface->intensity_compensation_top;
			lumscale1_single_bwd = gen7_vc1_top_surface->luma_scale_top[0];
			lumshift1_single_bwd = gen7_vc1_top_surface->luma_shift_top[0];
			replication_mode |= !!gen7_vc1_top_surface->frame_coding_mode << 1;
		}

		if (pic_param->sequence_fields.bits.interlace &&
			pic_param->picture_fields.bits.frame_coding_mode > 0) { /* Frame-Interlace or Field-Interlace */
			if (gen7_mfd_context->reference_surface[3].surface_id != VA_INVALID_ID)
				gen7_vc1_bottom_surface = (struct gen7_vc1_surface *)(gen7_mfd_context->reference_surface[3].obj_surface->private_data);
			else
				gen7_vc1_bottom_surface = NULL;

			if (gen7_vc1_bottom_surface) {
				intensitycomp_single_bwd |= !!gen7_vc1_bottom_surface->intensity_compensation_bottom << 1;
				lumscale2_single_bwd = gen7_vc1_bottom_surface->luma_scale_bottom[0];
				lumshift2_single_bwd = gen7_vc1_bottom_surface->luma_shift_bottom[0];
				replication_mode |= !!gen7_vc1_bottom_surface->frame_coding_mode << 3;
			}
		}
	}

	BEGIN_BCS_BATCH(batch, 6);
	OUT_BCS_BATCH(batch, MFX_VC1_PRED_PIPE_STATE | (6 - 2));
	OUT_BCS_BATCH(batch,
				  intensitycomp_double_fwd << 14 |
				  0 << 12 |
				  intensitycomp_single_fwd << 10 |
				  intensitycomp_single_bwd << 8 |
				  replication_mode << 4 |
				  0);
	OUT_BCS_BATCH(batch,
				  lumshift2_single_fwd << 24 |
				  lumshift1_single_fwd << 16 |
				  lumscale2_single_fwd << 8 |
				  lumscale1_single_fwd << 0);
	OUT_BCS_BATCH(batch,
				  lumshift2_double_fwd << 24 |
				  lumshift1_double_fwd << 16 |
				  lumscale2_double_fwd << 8 |
				  lumscale1_double_fwd << 0);
	OUT_BCS_BATCH(batch,
				  lumshift2_single_bwd << 24 |
				  lumshift1_single_bwd << 16 |
				  lumscale2_single_bwd << 8 |
				  lumscale1_single_bwd << 0);
	OUT_BCS_BATCH(batch,
				  0 << 24 |
				  0 << 16 |
				  0 << 8 |
				  0 << 0);
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_vc1_directmode_state(VADriverContextP ctx,
							  struct decode_state *decode_state,
							  struct gen7_mfd_context *gen7_mfd_context)
{
	struct i965_driver_data *i965 = i965_driver_data(ctx);
	VAPictureParameterBufferVC1 *pic_param;
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	struct object_surface *obj_surface;
	dri_bo *dmv_read_buffer = NULL, *dmv_write_buffer = NULL;
	int picture_type;
	int is_first_field = 1;

	pic_param = (VAPictureParameterBufferVC1 *)decode_state->pic_param->buffer;

	if (!pic_param->sequence_fields.bits.interlace ||
		(pic_param->picture_fields.bits.frame_coding_mode < 2)) { /* Progressive or Frame-Interlace */
		picture_type = pic_param->picture_fields.bits.picture_type;
	} else {/* Field-Interlace */
		is_first_field = pic_param->picture_fields.bits.is_first_field;
		picture_type = fptype_to_picture_type[pic_param->picture_fields.bits.picture_type][!is_first_field];
	}

	if (picture_type == GEN7_VC1_P_PICTURE ||
		picture_type == GEN7_VC1_SKIPPED_PICTURE) {
		obj_surface = decode_state->render_object;

		if (pic_param->sequence_fields.bits.interlace &&
			(pic_param->picture_fields.bits.frame_coding_mode == 2) && /* Field-Interlace */
			(pic_param->picture_fields.bits.top_field_first ^ is_first_field))
			dmv_write_buffer = ((struct gen7_vc1_surface *)(obj_surface->private_data))->dmv_bottom;
		else
			dmv_write_buffer = ((struct gen7_vc1_surface *)(obj_surface->private_data))->dmv_top;
	}

	if (picture_type == GEN7_VC1_B_PICTURE) {
		obj_surface = decode_state->reference_objects[1];
		if (pic_param->backward_reference_picture != VA_INVALID_ID &&
			obj_surface &&
			obj_surface->private_data) {

			if (pic_param->sequence_fields.bits.interlace &&
				(pic_param->picture_fields.bits.frame_coding_mode == 2) && /* Field-Interlace */
				(pic_param->picture_fields.bits.top_field_first ^ is_first_field))
				dmv_read_buffer = ((struct gen7_vc1_surface *)(obj_surface->private_data))->dmv_bottom;
			else
				dmv_read_buffer = ((struct gen7_vc1_surface *)(obj_surface->private_data))->dmv_top;
		}
	}

	BEGIN_BCS_BATCH(batch, 7);
	OUT_BCS_BATCH(batch, MFX_VC1_DIRECTMODE_STATE | (7 - 2));

	if (dmv_write_buffer)
		OUT_BCS_RELOC64(batch, dmv_write_buffer,
						I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
						0);
	else {
		OUT_BCS_BATCH(batch, 0);
		OUT_BCS_BATCH(batch, 0);
	}

	OUT_BCS_BATCH(batch, i965->intel.mocs_state);

	if (dmv_read_buffer)
		OUT_BCS_RELOC64(batch, dmv_read_buffer,
						I915_GEM_DOMAIN_INSTRUCTION, 0,
						0);
	else {
		OUT_BCS_BATCH(batch, 0);
		OUT_BCS_BATCH(batch, 0);
	}

	OUT_BCS_BATCH(batch, i965->intel.mocs_state);

	ADVANCE_BCS_BATCH(batch);
}

static int
gen8_mfd_vc1_get_macroblock_bit_offset(uint8_t *buf, int in_slice_data_bit_offset, int profile)
{
	int out_slice_data_bit_offset;
	int slice_header_size = in_slice_data_bit_offset / 8;
	int i, j;

	if (profile == 3 && slice_header_size) { /* Advanced Profile */
		for (i = 0, j = 0; i < slice_header_size - 1; i++, j++)
			if (!buf[j] && !buf[j + 1] && buf[j + 2] == 3 && buf[j + 3] < 4)
					i++, j += 2;

		if (i == slice_header_size - 1) {
			if (!buf[j] && !buf[j + 1] && buf[j + 2] == 3 && buf[j + 3] < 4) {
				buf[j + 2] = 0;
				j++;
			}

			j++;
		}

		out_slice_data_bit_offset = 8 * j + in_slice_data_bit_offset % 8;
	} else /* Simple or Main Profile */
		out_slice_data_bit_offset = in_slice_data_bit_offset;

	return out_slice_data_bit_offset;
}

static void
gen8_mfd_vc1_bsd_object(VADriverContextP ctx,
						VAPictureParameterBufferVC1 *pic_param,
						VASliceParameterBufferVC1 *slice_param,
						VASliceParameterBufferVC1 *next_slice_param,
						dri_bo *slice_data_bo,
						struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	int next_slice_start_vert_pos;
	int macroblock_offset;
	uint8_t *slice_data = NULL;

	dri_bo_map(slice_data_bo, True);
	slice_data = (uint8_t *)(slice_data_bo->virtual + slice_param->slice_data_offset);
	macroblock_offset = gen8_mfd_vc1_get_macroblock_bit_offset(slice_data,
															   slice_param->macroblock_offset,
															   pic_param->sequence_fields.bits.profile);
	dri_bo_unmap(slice_data_bo);

	if (next_slice_param)
		next_slice_start_vert_pos = next_slice_param->slice_vertical_position;
	else if (!pic_param->sequence_fields.bits.interlace ||
			 pic_param->picture_fields.bits.frame_coding_mode < 2) /* Progressive or Frame-Interlace */
		next_slice_start_vert_pos = ALIGN(pic_param->coded_height, 16) / 16;
	else /* Field-Interlace */
		next_slice_start_vert_pos = ALIGN(pic_param->coded_height, 32) / 32;

	BEGIN_BCS_BATCH(batch, 5);
	OUT_BCS_BATCH(batch, MFD_VC1_BSD_OBJECT | (5 - 2));
	OUT_BCS_BATCH(batch,
				  slice_param->slice_data_size - (macroblock_offset >> 3));
	OUT_BCS_BATCH(batch,
				  slice_param->slice_data_offset + (macroblock_offset >> 3));
	OUT_BCS_BATCH(batch,
				  slice_param->slice_vertical_position << 16 |
				  next_slice_start_vert_pos << 0);
	OUT_BCS_BATCH(batch,
				  (macroblock_offset & 0x7));
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_vc1_decode_picture(VADriverContextP ctx,
							struct decode_state *decode_state,
							struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	VAPictureParameterBufferVC1 *pic_param;
	VASliceParameterBufferVC1 *slice_param, *next_slice_param, *next_slice_group_param;
	dri_bo *slice_data_bo;
	int i, j;

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferVC1 *)decode_state->pic_param->buffer;

	gen8_mfd_vc1_decode_init(ctx, decode_state, gen7_mfd_context);
	intel_batchbuffer_start_atomic_bcs(batch, 0x1000);
	intel_batchbuffer_emit_mi_flush(batch);
	gen8_mfd_pipe_mode_select(ctx, decode_state, MFX_FORMAT_VC1, gen7_mfd_context);
	gen8_mfd_surface_state(ctx, decode_state, MFX_FORMAT_VC1, gen7_mfd_context);
	gen8_mfd_pipe_buf_addr_state(ctx, decode_state, MFX_FORMAT_VC1, gen7_mfd_context);
	gen8_mfd_bsp_buf_base_addr_state(ctx, decode_state, MFX_FORMAT_VC1, gen7_mfd_context);
	gen8_mfd_vc1_pic_state(ctx, decode_state, gen7_mfd_context);
	gen8_mfd_vc1_pred_pipe_state(ctx, decode_state, gen7_mfd_context);
	gen8_mfd_vc1_directmode_state(ctx, decode_state, gen7_mfd_context);

	for (j = 0; j < decode_state->num_slice_params; j++) {
		assert(decode_state->slice_params && decode_state->slice_params[j]->buffer);
		slice_param = (VASliceParameterBufferVC1 *)decode_state->slice_params[j]->buffer;
		slice_data_bo = decode_state->slice_datas[j]->bo;
		gen8_mfd_ind_obj_base_addr_state(ctx, slice_data_bo, MFX_FORMAT_VC1, gen7_mfd_context);

		if (j == decode_state->num_slice_params - 1)
			next_slice_group_param = NULL;
		else
			next_slice_group_param = (VASliceParameterBufferVC1 *)decode_state->slice_params[j + 1]->buffer;

		for (i = 0; i < decode_state->slice_params[j]->num_elements; i++) {
			assert(slice_param->slice_data_flag == VA_SLICE_DATA_FLAG_ALL);

			if (i < decode_state->slice_params[j]->num_elements - 1)
				next_slice_param = slice_param + 1;
			else
				next_slice_param = next_slice_group_param;

			gen8_mfd_vc1_bsd_object(ctx, pic_param, slice_param, next_slice_param, slice_data_bo, gen7_mfd_context);
			slice_param++;
		}
	}

	intel_batchbuffer_end_atomic(batch);
	intel_batchbuffer_flush(batch);
}

static void
gen8_mfd_jpeg_decode_init(VADriverContextP ctx,
						  struct decode_state *decode_state,
						  struct gen7_mfd_context *gen7_mfd_context)
{
	struct object_surface *obj_surface;
	VAPictureParameterBufferJPEGBaseline *pic_param;
	int subsampling = SUBSAMPLE_YUV420;
	int fourcc = VA_FOURCC_IMC3;

	pic_param = (VAPictureParameterBufferJPEGBaseline *)decode_state->pic_param->buffer;

	if (pic_param->num_components == 1) {
		subsampling = SUBSAMPLE_YUV400;
		fourcc = VA_FOURCC_Y800;
	} else if (pic_param->num_components == 3) {
		int h1 = pic_param->components[0].h_sampling_factor;
		int h2 = pic_param->components[1].h_sampling_factor;
		int h3 = pic_param->components[2].h_sampling_factor;
		int v1 = pic_param->components[0].v_sampling_factor;
		int v2 = pic_param->components[1].v_sampling_factor;
		int v3 = pic_param->components[2].v_sampling_factor;

		if (h1 == 2 * h2 && h2 == h3 &&
			v1 == 2 * v2 && v2 == v3) {
			subsampling = SUBSAMPLE_YUV420;
			fourcc = VA_FOURCC_IMC3;
		} else if (h1 == 2 * h2  && h2 == h3 &&
				   v1 == v2 && v2 == v3) {
			subsampling = SUBSAMPLE_YUV422H;
			fourcc = VA_FOURCC_422H;
		} else if (h1 == h2 && h2 == h3 &&
				   v1 == v2  && v2 == v3) {
			subsampling = SUBSAMPLE_YUV444;
			fourcc = VA_FOURCC_444P;
		} else if (h1 == 4 * h2 && h2 ==  h3 &&
				   v1 == v2 && v2 == v3) {
			subsampling = SUBSAMPLE_YUV411;
			fourcc = VA_FOURCC_411P;
		} else if (h1 == h2 && h2 == h3 &&
				   v1 == 2 * v2 && v2 == v3) {
			subsampling = SUBSAMPLE_YUV422V;
			fourcc = VA_FOURCC_422V;
		} else
			assert(0);
	} else {
		assert(0);
	}

	/* Current decoded picture */
	obj_surface = decode_state->render_object;
	i965_check_alloc_surface_bo(ctx, obj_surface, 1, fourcc, subsampling);

	dri_bo_unreference(gen7_mfd_context->pre_deblocking_output.bo);
	gen7_mfd_context->pre_deblocking_output.bo = obj_surface->bo;
	dri_bo_reference(gen7_mfd_context->pre_deblocking_output.bo);
	gen7_mfd_context->pre_deblocking_output.valid = 1;

	gen7_mfd_context->post_deblocking_output.bo = NULL;
	gen7_mfd_context->post_deblocking_output.valid = 0;

	gen7_mfd_context->intra_row_store_scratch_buffer.bo = NULL;
	gen7_mfd_context->intra_row_store_scratch_buffer.valid = 0;

	gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.bo = NULL;
	gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.valid = 0;

	gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.bo = NULL;
	gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.valid = 0;

	gen7_mfd_context->mpr_row_store_scratch_buffer.bo = NULL;
	gen7_mfd_context->mpr_row_store_scratch_buffer.valid = 0;

	gen7_mfd_context->bitplane_read_buffer.bo = NULL;
	gen7_mfd_context->bitplane_read_buffer.valid = 0;
}

static const int va_to_gen7_jpeg_rotation[4] = {
	GEN7_JPEG_ROTATION_0,
	GEN7_JPEG_ROTATION_90,
	GEN7_JPEG_ROTATION_180,
	GEN7_JPEG_ROTATION_270
};

static void
gen8_mfd_jpeg_pic_state(VADriverContextP ctx,
						struct decode_state *decode_state,
						struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	VAPictureParameterBufferJPEGBaseline *pic_param;
	int chroma_type = GEN7_YUV420;
	int frame_width_in_blks;
	int frame_height_in_blks;

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferJPEGBaseline *)decode_state->pic_param->buffer;

	if (pic_param->num_components == 1)
		chroma_type = GEN7_YUV400;
	else if (pic_param->num_components == 3) {
		int h1 = pic_param->components[0].h_sampling_factor;
		int h2 = pic_param->components[1].h_sampling_factor;
		int h3 = pic_param->components[2].h_sampling_factor;
		int v1 = pic_param->components[0].v_sampling_factor;
		int v2 = pic_param->components[1].v_sampling_factor;
		int v3 = pic_param->components[2].v_sampling_factor;

		if (h1 == 2 * h2 && h2 == h3 &&
			v1 == 2 * v2 && v2 == v3)
			chroma_type = GEN7_YUV420;
		else if (h1 == 2 && h2 == 1 && h3 == 1 &&
				 v1 == 1 && v2 == 1 && v3 == 1)
			chroma_type = GEN7_YUV422H_2Y;
		else if (h1 == h2 && h2 == h3 &&
				 v1 == v2 && v2 == v3)
			chroma_type = GEN7_YUV444;
		else if (h1 == 4 * h2 && h2 == h3 &&
				 v1 == v2 && v2 == v3)
			chroma_type = GEN7_YUV411;
		else if (h1 == 1 && h2 == 1 && h3 == 1 &&
				 v1 == 2 && v2 == 1 && v3 == 1)
			chroma_type = GEN7_YUV422V_2Y;
		else if (h1 == 2 && h2 == 1 && h3 == 1 &&
				 v1 == 2 && v2 == 2 && v3 == 2)
			chroma_type = GEN7_YUV422H_4Y;
		else if (h1 == 2 && h2 == 2 && h3 == 2 &&
				 v1 == 2 && v2 == 1 && v3 == 1)
			chroma_type = GEN7_YUV422V_4Y;
		else
			assert(0);
	}

	if (chroma_type == GEN7_YUV400 ||
		chroma_type == GEN7_YUV444 ||
		chroma_type == GEN7_YUV422V_2Y) {
		frame_width_in_blks = ((pic_param->picture_width + 7) / 8);
		frame_height_in_blks = ((pic_param->picture_height + 7) / 8);
	} else if (chroma_type == GEN7_YUV411) {
		frame_width_in_blks = ((pic_param->picture_width + 31) / 32) * 4;
		frame_height_in_blks = ((pic_param->picture_height + 31) / 32) * 4;
	} else {
		frame_width_in_blks = ((pic_param->picture_width + 15) / 16) * 2;
		frame_height_in_blks = ((pic_param->picture_height + 15) / 16) * 2;
	}

	BEGIN_BCS_BATCH(batch, 3);
	OUT_BCS_BATCH(batch, MFX_JPEG_PIC_STATE | (3 - 2));
	OUT_BCS_BATCH(batch,
				  (va_to_gen7_jpeg_rotation[0] << 4) |    /* without rotation */
				  (chroma_type << 0));
	OUT_BCS_BATCH(batch,
				  ((frame_height_in_blks - 1) << 16) |   /* FrameHeightInBlks */
				  ((frame_width_in_blks - 1) << 0));    /* FrameWidthInBlks */
	ADVANCE_BCS_BATCH(batch);
}

static const int va_to_gen7_jpeg_hufftable[2] = {
	MFX_HUFFTABLE_ID_Y,
	MFX_HUFFTABLE_ID_UV
};

static void
gen8_mfd_jpeg_huff_table_state(VADriverContextP ctx,
							   struct decode_state *decode_state,
							   struct gen7_mfd_context *gen7_mfd_context,
							   int num_tables)
{
	VAHuffmanTableBufferJPEGBaseline *huffman_table;
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	int index;

	if (!decode_state->huffman_table || !decode_state->huffman_table->buffer)
		return;

	huffman_table = (VAHuffmanTableBufferJPEGBaseline *)decode_state->huffman_table->buffer;

	for (index = 0; index < num_tables; index++) {
		int id = va_to_gen7_jpeg_hufftable[index];
		if (!huffman_table->load_huffman_table[index])
			continue;
		BEGIN_BCS_BATCH(batch, 53);
		OUT_BCS_BATCH(batch, MFX_JPEG_HUFF_TABLE_STATE | (53 - 2));
		OUT_BCS_BATCH(batch, id);
		intel_batchbuffer_data(batch, huffman_table->huffman_table[index].num_dc_codes, 12);
		intel_batchbuffer_data(batch, huffman_table->huffman_table[index].dc_values, 12);
		intel_batchbuffer_data(batch, huffman_table->huffman_table[index].num_ac_codes, 16);
		intel_batchbuffer_data(batch, huffman_table->huffman_table[index].ac_values, 164);
		ADVANCE_BCS_BATCH(batch);
	}
}

static const int va_to_gen7_jpeg_qm[5] = {
	-1,
	MFX_QM_JPEG_LUMA_Y_QUANTIZER_MATRIX,
	MFX_QM_JPEG_CHROMA_CB_QUANTIZER_MATRIX,
	MFX_QM_JPEG_CHROMA_CR_QUANTIZER_MATRIX,
	MFX_QM_JPEG_ALPHA_QUANTIZER_MATRIX
};

static void
gen8_mfd_jpeg_qm_state(VADriverContextP ctx,
					   struct decode_state *decode_state,
					   struct gen7_mfd_context *gen7_mfd_context)
{
	VAPictureParameterBufferJPEGBaseline *pic_param;
	VAIQMatrixBufferJPEGBaseline *iq_matrix;
	int index;

	if (!decode_state->iq_matrix || !decode_state->iq_matrix->buffer)
		return;

	iq_matrix = (VAIQMatrixBufferJPEGBaseline *)decode_state->iq_matrix->buffer;
	pic_param = (VAPictureParameterBufferJPEGBaseline *)decode_state->pic_param->buffer;

	assert(pic_param->num_components <= 3);

	for (index = 0; index < pic_param->num_components; index++) {
		unsigned char id = pic_param->components[index].component_id - pic_param->components[0].component_id + 1;
		int qm_type;
		unsigned char *qm = iq_matrix->quantiser_table[pic_param->components[index].quantiser_table_selector];
		unsigned char raster_qm[64];
		int j;

		if (id > 4 || id < 1)
			continue;

		if (!iq_matrix->load_quantiser_table[pic_param->components[index].quantiser_table_selector])
			continue;

		qm_type = va_to_gen7_jpeg_qm[id];

		for (j = 0; j < 64; j++)
			raster_qm[zigzag_direct[j]] = qm[j];

		gen8_mfd_qm_state(ctx, qm_type, raster_qm, 64, gen7_mfd_context);
	}
}

static void
gen8_mfd_jpeg_bsd_object(VADriverContextP ctx,
						 VAPictureParameterBufferJPEGBaseline *pic_param,
						 VASliceParameterBufferJPEGBaseline *slice_param,
						 VASliceParameterBufferJPEGBaseline *next_slice_param,
						 dri_bo *slice_data_bo,
						 struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	int scan_component_mask = 0;
	unsigned char id;
	int i;

	assert(slice_param->num_components > 0);
	assert(slice_param->num_components < 4);
	assert(slice_param->num_components <= pic_param->num_components);

	for (i = 0; i < slice_param->num_components; i++) {
		id = slice_param->components[i].component_selector - pic_param->components[0].component_id + 1;
		switch (id)
		{
		case 1:
			scan_component_mask |= (1 << 0);
			break;
		case 2:
			scan_component_mask |= (1 << 1);
			break;
		case 3:
			scan_component_mask |= (1 << 2);
			break;
		default:
			assert(0);
			break;
		}
	}

	BEGIN_BCS_BATCH(batch, 6);
	OUT_BCS_BATCH(batch, MFD_JPEG_BSD_OBJECT | (6 - 2));
	OUT_BCS_BATCH(batch,
				  slice_param->slice_data_size);
	OUT_BCS_BATCH(batch,
				  slice_param->slice_data_offset);
	OUT_BCS_BATCH(batch,
				  slice_param->slice_horizontal_position << 16 |
				  slice_param->slice_vertical_position << 0);
	OUT_BCS_BATCH(batch,
				  ((slice_param->num_components != 1) << 30) |  /* interleaved */
				  (scan_component_mask << 27) |                 /* scan components */
				  (0 << 26) |   /* disable interrupt allowed */
				  (slice_param->num_mcus << 0));                /* MCU count */
	OUT_BCS_BATCH(batch,
				  (slice_param->restart_interval << 0));    /* RestartInterval */
	ADVANCE_BCS_BATCH(batch);
}

/* Workaround for JPEG decoding on Ivybridge */
#ifdef JPEG_WA

static struct {
	int width;
	int height;
	unsigned char data[32];
	int data_size;
	int data_bit_offset;
	int qp;
} gen7_jpeg_wa_clip = {
	16,
	16,
	{
		0x65, 0xb8, 0x40, 0x32, 0x13, 0xfd, 0x06, 0x6c,
		0xfc, 0x0a, 0x50, 0x71, 0x5c, 0x00
	},
	14,
	40,
	28,
};

static void
gen8_jpeg_wa_init(VADriverContextP ctx,
				  struct gen7_mfd_context *gen7_mfd_context)
{
	struct i965_driver_data *i965 = i965_driver_data(ctx);
	VAStatus status;
	struct object_surface *obj_surface;

	if (gen7_mfd_context->jpeg_wa_surface_id != VA_INVALID_SURFACE)
		i965_DestroySurfaces(ctx,
							 &gen7_mfd_context->jpeg_wa_surface_id,
							 1);

	status = i965_CreateSurfaces(ctx,
								 gen7_jpeg_wa_clip.width,
								 gen7_jpeg_wa_clip.height,
								 VA_RT_FORMAT_YUV420,
								 1,
								 &gen7_mfd_context->jpeg_wa_surface_id);
	assert(status == VA_STATUS_SUCCESS);

	obj_surface = SURFACE(gen7_mfd_context->jpeg_wa_surface_id);
	assert(obj_surface);
	i965_check_alloc_surface_bo(ctx, obj_surface, 1, VA_FOURCC_NV12, SUBSAMPLE_YUV420);
	gen7_mfd_context->jpeg_wa_surface_object = obj_surface;

	if (!gen7_mfd_context->jpeg_wa_slice_data_bo) {
		gen7_mfd_context->jpeg_wa_slice_data_bo = dri_bo_alloc(i965->intel.bufmgr,
															   "JPEG WA data",
															   0x1000,
															   0x1000);
		dri_bo_subdata(gen7_mfd_context->jpeg_wa_slice_data_bo,
					   0,
					   gen7_jpeg_wa_clip.data_size,
					   gen7_jpeg_wa_clip.data);
	}
}

static void
gen8_jpeg_wa_pipe_mode_select(VADriverContextP ctx,
							  struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;

	BEGIN_BCS_BATCH(batch, 5);
	OUT_BCS_BATCH(batch, MFX_PIPE_MODE_SELECT | (5 - 2));
	OUT_BCS_BATCH(batch,
				  (gen7_mfd_context->decoder_format_mode << 17) | /* Currently only support long format */
				  (MFD_MODE_VLD << 15) | /* VLD mode */
				  (0 << 10) | /* disable Stream-Out */
				  (0 << 9)  | /* Post Deblocking Output */
				  (1 << 8)  | /* Pre Deblocking Output */
				  (0 << 5)  | /* not in stitch mode */
				  (MFX_CODEC_DECODE << 4)  | /* decoding mode */
				  (MFX_FORMAT_AVC << 0));
	OUT_BCS_BATCH(batch,
				  (0 << 4)  | /* terminate if AVC motion and POC table error occurs */
				  (0 << 3)  | /* terminate if AVC mbdata error occurs */
				  (0 << 2)  | /* terminate if AVC CABAC/CAVLC decode error occurs */
				  (0 << 1)  |
				  (0 << 0));
	OUT_BCS_BATCH(batch, 0); /* pic status/error report id */
	OUT_BCS_BATCH(batch, 0); /* reserved */
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_jpeg_wa_surface_state(VADriverContextP ctx,
						   struct gen7_mfd_context *gen7_mfd_context)
{
	struct object_surface *obj_surface = gen7_mfd_context->jpeg_wa_surface_object;
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;

	BEGIN_BCS_BATCH(batch, 6);
	OUT_BCS_BATCH(batch, MFX_SURFACE_STATE | (6 - 2));
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch,
				  ((obj_surface->orig_width - 1) << 18) |
				  ((obj_surface->orig_height - 1) << 4));
	OUT_BCS_BATCH(batch,
				  (MFX_SURFACE_PLANAR_420_8 << 28) | /* 420 planar YUV surface */
				  (1 << 27) | /* interleave chroma, set to 0 for JPEG */
				  (0 << 22) | /* surface object control state, ignored */
				  ((obj_surface->width - 1) << 3) | /* pitch */
				  (0 << 2)  | /* must be 0 */
				  (1 << 1)  | /* must be tiled */
				  (I965_TILEWALK_YMAJOR << 0));  /* tile walk, must be 1 */
	OUT_BCS_BATCH(batch,
				  (0 << 16) | /* X offset for U(Cb), must be 0 */
				  (obj_surface->y_cb_offset << 0)); /* Y offset for U(Cb) */
	OUT_BCS_BATCH(batch,
				  (0 << 16) | /* X offset for V(Cr), must be 0 */
				  (0 << 0)); /* Y offset for V(Cr), must be 0 for video codec, non-zoro for JPEG */
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_jpeg_wa_pipe_buf_addr_state(VADriverContextP ctx,
								 struct gen7_mfd_context *gen7_mfd_context)
{
	struct i965_driver_data *i965 = i965_driver_data(ctx);
	struct object_surface *obj_surface = gen7_mfd_context->jpeg_wa_surface_object;
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	dri_bo *intra_bo;
	int i;

	intra_bo = dri_bo_alloc(i965->intel.bufmgr,
							"intra row store",
							128 * 64,
							0x1000);

	BEGIN_BCS_BATCH(batch, 61);
	OUT_BCS_BATCH(batch, MFX_PIPE_BUF_ADDR_STATE | (61 - 2));
	OUT_BCS_RELOC64(batch,
					obj_surface->bo,
					I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
					0);
	OUT_BCS_BATCH(batch, i965->intel.mocs_state);


	OUT_BCS_BATCH(batch, 0); /* post deblocking */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	/* uncompressed-video & stream out 7-12 */
	OUT_BCS_BATCH(batch, 0); /* ignore for decoding */
	OUT_BCS_BATCH(batch, 0); /* ignore for decoding */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	/* the DW 13-15 is for intra row store scratch */
	OUT_BCS_RELOC64(batch,
					intra_bo,
					I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
					0);

	OUT_BCS_BATCH(batch, i965->intel.mocs_state);

	/* the DW 16-18 is for deblocking filter */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	/* DW 19..50 */
	for (i = 0; i < MAX_GEN_REFERENCE_FRAMES; i++) {
		OUT_BCS_BATCH(batch, 0);
		OUT_BCS_BATCH(batch, 0);
	}
	OUT_BCS_BATCH(batch, 0);

	/* the DW52-54 is for mb status address */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	/* the DW56-60 is for ILDB & second ILDB address */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	ADVANCE_BCS_BATCH(batch);

	dri_bo_unreference(intra_bo);
}

static void
gen8_jpeg_wa_bsp_buf_base_addr_state(VADriverContextP ctx,
									 struct gen7_mfd_context *gen7_mfd_context)
{
	struct i965_driver_data *i965 = i965_driver_data(ctx);
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	dri_bo *bsd_mpc_bo, *mpr_bo;

	bsd_mpc_bo = dri_bo_alloc(i965->intel.bufmgr,
							  "bsd mpc row store",
							  11520, /* 1.5 * 120 * 64 */
							  0x1000);

	mpr_bo = dri_bo_alloc(i965->intel.bufmgr,
						  "mpr row store",
						  7680, /* 1. 0 * 120 * 64 */
						  0x1000);

	BEGIN_BCS_BATCH(batch, 10);
	OUT_BCS_BATCH(batch, MFX_BSP_BUF_BASE_ADDR_STATE | (10 - 2));

	OUT_BCS_RELOC64(batch,
					bsd_mpc_bo,
					I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
					0);

	OUT_BCS_BATCH(batch, i965->intel.mocs_state);

	OUT_BCS_RELOC64(batch,
					mpr_bo,
					I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
					0);
	OUT_BCS_BATCH(batch, i965->intel.mocs_state);

	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	ADVANCE_BCS_BATCH(batch);

	dri_bo_unreference(bsd_mpc_bo);
	dri_bo_unreference(mpr_bo);
}

static void
gen8_jpeg_wa_avc_qm_state(VADriverContextP ctx,
						  struct gen7_mfd_context *gen7_mfd_context)
{

}

static void
gen8_jpeg_wa_avc_img_state(VADriverContextP ctx,
						   struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	int img_struct = 0;
	int mbaff_frame_flag = 0;
	unsigned int width_in_mbs = 1, height_in_mbs = 1;

	BEGIN_BCS_BATCH(batch, 16);
	OUT_BCS_BATCH(batch, MFX_AVC_IMG_STATE | (16 - 2));
	OUT_BCS_BATCH(batch,
				  width_in_mbs * height_in_mbs);
	OUT_BCS_BATCH(batch,
				  ((height_in_mbs - 1) << 16) |
				  ((width_in_mbs - 1) << 0));
	OUT_BCS_BATCH(batch,
				  (0 << 24) |
				  (0 << 16) |
				  (0 << 14) |
				  (0 << 13) |
				  (0 << 12) | /* differ from GEN6 */
				  (0 << 10) |
				  (img_struct << 8));
	OUT_BCS_BATCH(batch,
				  (1 << 10) | /* 4:2:0 */
				  (1 << 7) |  /* CABAC */
				  (0 << 6) |
				  (0 << 5) |
				  (0 << 4) |
				  (0 << 3) |
				  (1 << 2) |
				  (mbaff_frame_flag << 1) |
				  (0 << 0));
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_jpeg_wa_avc_directmode_state(VADriverContextP ctx,
								  struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	int i;

	BEGIN_BCS_BATCH(batch, 71);
	OUT_BCS_BATCH(batch, MFX_AVC_DIRECTMODE_STATE | (71 - 2));

	/* reference surfaces 0..15 */
	for (i = 0; i < MAX_GEN_REFERENCE_FRAMES; i++) {
		OUT_BCS_BATCH(batch, 0); /* top */
		OUT_BCS_BATCH(batch, 0); /* bottom */
	}

	OUT_BCS_BATCH(batch, 0);

	/* the current decoding frame/field */
	OUT_BCS_BATCH(batch, 0); /* top */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	/* POC List */
	for (i = 0; i < MAX_GEN_REFERENCE_FRAMES; i++) {
		OUT_BCS_BATCH(batch, 0);
		OUT_BCS_BATCH(batch, 0);
	}

	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);

	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_jpeg_wa_ind_obj_base_addr_state(VADriverContextP ctx,
									 struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;

	BEGIN_BCS_BATCH(batch, 11);
	OUT_BCS_BATCH(batch, MFX_IND_OBJ_BASE_ADDR_STATE | (11 - 2));
	OUT_BCS_RELOC64(batch,
					gen7_mfd_context->jpeg_wa_slice_data_bo,
					I915_GEM_DOMAIN_INSTRUCTION, 0,
					0);
	OUT_BCS_BATCH(batch, i965->intel.mocs_state);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0); /* ignore for VLD mode */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0); /* ignore for VLD mode */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0); /* ignore for VLD mode */
	OUT_BCS_BATCH(batch, 0);
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_jpeg_wa_avc_bsd_object(VADriverContextP ctx,
							struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;

	/* the input bitsteam format on GEN7 differs from GEN6 */
	BEGIN_BCS_BATCH(batch, 6);
	OUT_BCS_BATCH(batch, MFD_AVC_BSD_OBJECT | (6 - 2));
	OUT_BCS_BATCH(batch, gen7_jpeg_wa_clip.data_size);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch,
				  (0 << 31) |
				  (0 << 14) |
				  (0 << 12) |
				  (0 << 10) |
				  (0 << 8));
	OUT_BCS_BATCH(batch,
				  ((gen7_jpeg_wa_clip.data_bit_offset >> 3) << 16) |
				  (0 << 5)  |
				  (0 << 4)  |
				  (1 << 3) | /* LastSlice Flag */
				  (gen7_jpeg_wa_clip.data_bit_offset & 0x7));
	OUT_BCS_BATCH(batch, 0);
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_jpeg_wa_avc_slice_state(VADriverContextP ctx,
							 struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	int slice_hor_pos = 0, slice_ver_pos = 0, next_slice_hor_pos = 0, next_slice_ver_pos = 1;
	int num_ref_idx_l0 = 0, num_ref_idx_l1 = 0;
	int first_mb_in_slice = 0;
	int slice_type = SLICE_TYPE_I;

	BEGIN_BCS_BATCH(batch, 11);
	OUT_BCS_BATCH(batch, MFX_AVC_SLICE_STATE | (11 - 2));
	OUT_BCS_BATCH(batch, slice_type);
	OUT_BCS_BATCH(batch,
				  (num_ref_idx_l1 << 24) |
				  (num_ref_idx_l0 << 16) |
				  (0 << 8) |
				  (0 << 0));
	OUT_BCS_BATCH(batch,
				  (0 << 29) |
				  (1 << 27) |   /* disable Deblocking */
				  (0 << 24) |
				  (gen7_jpeg_wa_clip.qp << 16) |
				  (0 << 8) |
				  (0 << 0));
	OUT_BCS_BATCH(batch,
				  (slice_ver_pos << 24) |
				  (slice_hor_pos << 16) |
				  (first_mb_in_slice << 0));
	OUT_BCS_BATCH(batch,
				  (next_slice_ver_pos << 16) |
				  (next_slice_hor_pos << 0));
	OUT_BCS_BATCH(batch, (1 << 19)); /* last slice flag */
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	OUT_BCS_BATCH(batch, 0);
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_jpeg_wa(VADriverContextP ctx,
				 struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	gen8_jpeg_wa_init(ctx, gen7_mfd_context);
	intel_batchbuffer_emit_mi_flush(batch);
	gen8_jpeg_wa_pipe_mode_select(ctx, gen7_mfd_context);
	gen8_jpeg_wa_surface_state(ctx, gen7_mfd_context);
	gen8_jpeg_wa_pipe_buf_addr_state(ctx, gen7_mfd_context);
	gen8_jpeg_wa_bsp_buf_base_addr_state(ctx, gen7_mfd_context);
	gen8_jpeg_wa_avc_qm_state(ctx, gen7_mfd_context);
	gen8_jpeg_wa_avc_img_state(ctx, gen7_mfd_context);
	gen8_jpeg_wa_ind_obj_base_addr_state(ctx, gen7_mfd_context);

	gen8_jpeg_wa_avc_directmode_state(ctx, gen7_mfd_context);
	gen8_jpeg_wa_avc_slice_state(ctx, gen7_mfd_context);
	gen8_jpeg_wa_avc_bsd_object(ctx, gen7_mfd_context);
}

#endif

static void
gen8_mfd_jpeg_decode_picture(VADriverContextP ctx,
							 struct decode_state *decode_state,
							 struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	VAPictureParameterBufferJPEGBaseline *pic_param;
	VASliceParameterBufferJPEGBaseline *slice_param, *next_slice_param, *next_slice_group_param;
	dri_bo *slice_data_bo;
	int i, j, max_selector = 0;

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferJPEGBaseline *)decode_state->pic_param->buffer;

	/* Currently only support Baseline DCT */
	gen8_mfd_jpeg_decode_init(ctx, decode_state, gen7_mfd_context);
	intel_batchbuffer_start_atomic_bcs(batch, 0x1000);
#ifdef JPEG_WA
	gen8_mfd_jpeg_wa(ctx, gen7_mfd_context);
#endif
	intel_batchbuffer_emit_mi_flush(batch);
	gen8_mfd_pipe_mode_select(ctx, decode_state, MFX_FORMAT_JPEG, gen7_mfd_context);
	gen8_mfd_surface_state(ctx, decode_state, MFX_FORMAT_JPEG, gen7_mfd_context);
	gen8_mfd_pipe_buf_addr_state(ctx, decode_state, MFX_FORMAT_JPEG, gen7_mfd_context);
	gen8_mfd_jpeg_pic_state(ctx, decode_state, gen7_mfd_context);
	gen8_mfd_jpeg_qm_state(ctx, decode_state, gen7_mfd_context);

	for (j = 0; j < decode_state->num_slice_params; j++) {
		assert(decode_state->slice_params && decode_state->slice_params[j]->buffer);
		slice_param = (VASliceParameterBufferJPEGBaseline *)decode_state->slice_params[j]->buffer;
		slice_data_bo = decode_state->slice_datas[j]->bo;
		gen8_mfd_ind_obj_base_addr_state(ctx, slice_data_bo, MFX_FORMAT_JPEG, gen7_mfd_context);

		if (j == decode_state->num_slice_params - 1)
			next_slice_group_param = NULL;
		else
			next_slice_group_param = (VASliceParameterBufferJPEGBaseline *)decode_state->slice_params[j + 1]->buffer;

		for (i = 0; i < decode_state->slice_params[j]->num_elements; i++) {
			int component;

			assert(slice_param->slice_data_flag == VA_SLICE_DATA_FLAG_ALL);

			if (i < decode_state->slice_params[j]->num_elements - 1)
				next_slice_param = slice_param + 1;
			else
				next_slice_param = next_slice_group_param;

			for (component = 0; component < slice_param->num_components; component++) {
				if (max_selector < slice_param->components[component].dc_table_selector)
					max_selector = slice_param->components[component].dc_table_selector;

				if (max_selector < slice_param->components[component].ac_table_selector)
					max_selector = slice_param->components[component].ac_table_selector;
			}

			slice_param++;
		}
	}

	assert(max_selector < 2);
	gen8_mfd_jpeg_huff_table_state(ctx, decode_state, gen7_mfd_context, max_selector + 1);

	for (j = 0; j < decode_state->num_slice_params; j++) {
		assert(decode_state->slice_params && decode_state->slice_params[j]->buffer);
		slice_param = (VASliceParameterBufferJPEGBaseline *)decode_state->slice_params[j]->buffer;
		slice_data_bo = decode_state->slice_datas[j]->bo;
		gen8_mfd_ind_obj_base_addr_state(ctx, slice_data_bo, MFX_FORMAT_JPEG, gen7_mfd_context);

		if (j == decode_state->num_slice_params - 1)
			next_slice_group_param = NULL;
		else
			next_slice_group_param = (VASliceParameterBufferJPEGBaseline *)decode_state->slice_params[j + 1]->buffer;

		for (i = 0; i < decode_state->slice_params[j]->num_elements; i++) {
			assert(slice_param->slice_data_flag == VA_SLICE_DATA_FLAG_ALL);

			if (i < decode_state->slice_params[j]->num_elements - 1)
				next_slice_param = slice_param + 1;
			else
				next_slice_param = next_slice_group_param;

			gen8_mfd_jpeg_bsd_object(ctx, pic_param, slice_param, next_slice_param, slice_data_bo, gen7_mfd_context);
			slice_param++;
		}
	}

	intel_batchbuffer_end_atomic(batch);
	intel_batchbuffer_flush(batch);
}

static const int vp8_dc_qlookup[128] = {
	4,   5,   6,   7,   8,   9,  10,  10,  11,  12,  13,  14,  15,  16,  17,  17,
	18,  19,  20,  20,  21,  21,  22,  22,  23,  23,  24,  25,  25,  26,  27,  28,
	29,  30,  31,  32,  33,  34,  35,  36,  37,  37,  38,  39,  40,  41,  42,  43,
	44,  45,  46,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,
	59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
	75,  76,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,
	91,  93,  95,  96,  98, 100, 101, 102, 104, 106, 108, 110, 112, 114, 116, 118,
	122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 143, 145, 148, 151, 154, 157,
};

static const int vp8_ac_qlookup[128] = {
	4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
	20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,
	36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,
	52,  53,  54,  55,  56,  57,  58,  60,  62,  64,  66,  68,  70,  72,  74,  76,
	78,  80,  82,  84,  86,  88,  90,  92,  94,  96,  98, 100, 102, 104, 106, 108,
	110, 112, 114, 116, 119, 122, 125, 128, 131, 134, 137, 140, 143, 146, 149, 152,
	155, 158, 161, 164, 167, 170, 173, 177, 181, 185, 189, 193, 197, 201, 205, 209,
	213, 217, 221, 225, 229, 234, 239, 245, 249, 254, 259, 264, 269, 274, 279, 284,
};

static inline unsigned int vp8_clip_quantization_index(int index)
{
	if (index > 127)
		return 127;
	else if (index < 0)
		return 0;

	return index;
}

static void
gen8_mfd_vp8_decode_init(VADriverContextP ctx,
						 struct decode_state *decode_state,
						 struct gen7_mfd_context *gen7_mfd_context)
{
	struct object_surface *obj_surface;
	struct i965_driver_data *i965 = i965_driver_data(ctx);
	dri_bo *bo;
	VAPictureParameterBufferVP8 *pic_param = (VAPictureParameterBufferVP8 *)decode_state->pic_param->buffer;
	int width_in_mbs = (pic_param->frame_width + 15) / 16;
	int height_in_mbs = (pic_param->frame_height + 15) / 16;

	assert(width_in_mbs > 0 && width_in_mbs <= 256); /* 4K */
	assert(height_in_mbs > 0 && height_in_mbs <= 256);

	intel_update_vp8_frame_store_index(ctx,
									   decode_state,
									   pic_param,
									   gen7_mfd_context->reference_surface);

	/* Current decoded picture */
	obj_surface = decode_state->render_object;
	i965_check_alloc_surface_bo(ctx, obj_surface, 1, VA_FOURCC_NV12, SUBSAMPLE_YUV420);

	dri_bo_unreference(gen7_mfd_context->post_deblocking_output.bo);
	gen7_mfd_context->post_deblocking_output.bo = obj_surface->bo;
	dri_bo_reference(gen7_mfd_context->post_deblocking_output.bo);
	gen7_mfd_context->post_deblocking_output.valid = !pic_param->pic_fields.bits.loop_filter_disable;

	dri_bo_unreference(gen7_mfd_context->pre_deblocking_output.bo);
	gen7_mfd_context->pre_deblocking_output.bo = obj_surface->bo;
	dri_bo_reference(gen7_mfd_context->pre_deblocking_output.bo);
	gen7_mfd_context->pre_deblocking_output.valid = pic_param->pic_fields.bits.loop_filter_disable;

	intel_ensure_vp8_segmentation_buffer(ctx,
										 &gen7_mfd_context->segmentation_buffer, width_in_mbs, height_in_mbs);

	/* The same as AVC */
	dri_bo_unreference(gen7_mfd_context->intra_row_store_scratch_buffer.bo);
	bo = dri_bo_alloc(i965->intel.bufmgr,
					  "intra row store",
					  width_in_mbs * 64,
					  0x1000);
	assert(bo);
	gen7_mfd_context->intra_row_store_scratch_buffer.bo = bo;
	gen7_mfd_context->intra_row_store_scratch_buffer.valid = 1;

	dri_bo_unreference(gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.bo);
	bo = dri_bo_alloc(i965->intel.bufmgr,
					  "deblocking filter row store",
					  width_in_mbs * 64 * 4,
					  0x1000);
	assert(bo);
	gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.bo = bo;
	gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.valid = 1;

	dri_bo_unreference(gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.bo);
	bo = dri_bo_alloc(i965->intel.bufmgr,
					  "bsd mpc row store",
					  width_in_mbs * 64 * 2,
					  0x1000);
	assert(bo);
	gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.bo = bo;
	gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.valid = 1;

	dri_bo_unreference(gen7_mfd_context->mpr_row_store_scratch_buffer.bo);
	bo = dri_bo_alloc(i965->intel.bufmgr,
					  "mpr row store",
					  width_in_mbs * 64 * 2,
					  0x1000);
	assert(bo);
	gen7_mfd_context->mpr_row_store_scratch_buffer.bo = bo;
	gen7_mfd_context->mpr_row_store_scratch_buffer.valid = 1;

	gen7_mfd_context->bitplane_read_buffer.valid = 0;
}

static void
gen8_mfd_vp8_pic_state(VADriverContextP ctx,
					   struct decode_state *decode_state,
					   struct gen7_mfd_context *gen7_mfd_context)
{
	struct i965_driver_data *i965 = i965_driver_data(ctx);
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	VAPictureParameterBufferVP8 *pic_param = (VAPictureParameterBufferVP8 *)decode_state->pic_param->buffer;
	VAIQMatrixBufferVP8 *iq_matrix = (VAIQMatrixBufferVP8 *)decode_state->iq_matrix->buffer;
	VASliceParameterBufferVP8 *slice_param = (VASliceParameterBufferVP8 *)decode_state->slice_params[0]->buffer; /* one slice per frame */
	dri_bo *probs_bo = decode_state->probability_data->bo;
	int i, j, log2num;
	unsigned int quantization_value[4][6];

	/* There is no safe way to error out if the segmentation buffer
	   could not be allocated. So, instead of aborting, simply decode
	   something even if the result may look totally inacurate */
	const unsigned int enable_segmentation =
		pic_param->pic_fields.bits.segmentation_enabled &&
		gen7_mfd_context->segmentation_buffer.valid;

	log2num = (int)log2(slice_param->num_of_partitions - 1);

	BEGIN_BCS_BATCH(batch, 38);
	OUT_BCS_BATCH(batch, MFX_VP8_PIC_STATE | (38 - 2));
	OUT_BCS_BATCH(batch,
				  (ALIGN(pic_param->frame_height, 16) / 16 - 1) << 16 |
				  (ALIGN(pic_param->frame_width, 16) / 16 - 1) << 0);
	OUT_BCS_BATCH(batch,
				  log2num << 24 |
				  pic_param->pic_fields.bits.sharpness_level << 16 |
				  pic_param->pic_fields.bits.sign_bias_alternate << 13 |
				  pic_param->pic_fields.bits.sign_bias_golden << 12 |
				  pic_param->pic_fields.bits.loop_filter_adj_enable << 11 |
				  pic_param->pic_fields.bits.mb_no_coeff_skip << 10 |
				  (enable_segmentation &&
				   pic_param->pic_fields.bits.update_mb_segmentation_map) << 9 |
				  pic_param->pic_fields.bits.segmentation_enabled << 8 |
				  (enable_segmentation &&
				   !pic_param->pic_fields.bits.update_mb_segmentation_map) << 7 |
				  (enable_segmentation &&
				   pic_param->pic_fields.bits.update_mb_segmentation_map) << 6 |
				  (pic_param->pic_fields.bits.key_frame == 0 ? 1 : 0) << 5 |    /* 0 indicate an intra frame in VP8 stream/spec($9.1)*/
				  pic_param->pic_fields.bits.filter_type << 4 |
				  (pic_param->pic_fields.bits.version == 3) << 1 | /* full pixel mode for version 3 */
				  !!pic_param->pic_fields.bits.version << 0); /* version 0: 6 tap */

	OUT_BCS_BATCH(batch,
				  pic_param->loop_filter_level[3] << 24 |
				  pic_param->loop_filter_level[2] << 16 |
				  pic_param->loop_filter_level[1] <<  8 |
				  pic_param->loop_filter_level[0] <<  0);

	/* Quantizer Value for 4 segmetns, DW4-DW15 */
	for (i = 0; i < 4; i++) {
		quantization_value[i][0] = vp8_ac_qlookup[vp8_clip_quantization_index(iq_matrix->quantization_index[i][0])];/*yac*/
		quantization_value[i][1] = vp8_dc_qlookup[vp8_clip_quantization_index(iq_matrix->quantization_index[i][1])];/*ydc*/
		quantization_value[i][2] = 2 * vp8_dc_qlookup[vp8_clip_quantization_index(iq_matrix->quantization_index[i][2])]; /*y2dc*/
		/* 101581>>16 is equivalent to 155/100 */
		quantization_value[i][3] = (101581 * vp8_ac_qlookup[vp8_clip_quantization_index(iq_matrix->quantization_index[i][3])]) >> 16; /*y2ac*/
		quantization_value[i][4] = vp8_dc_qlookup[vp8_clip_quantization_index(iq_matrix->quantization_index[i][4])];/*uvdc*/
		quantization_value[i][5] = vp8_ac_qlookup[vp8_clip_quantization_index(iq_matrix->quantization_index[i][5])];/*uvac*/

		quantization_value[i][3] = (quantization_value[i][3] > 8 ? quantization_value[i][3] : 8);
		quantization_value[i][4] = (quantization_value[i][4] < 132 ? quantization_value[i][4] : 132);

		OUT_BCS_BATCH(batch,
					  quantization_value[i][0] << 16 | /* Y1AC */
					  quantization_value[i][1] <<  0); /* Y1DC */
		OUT_BCS_BATCH(batch,
					  quantization_value[i][5] << 16 | /* UVAC */
					  quantization_value[i][4] <<  0); /* UVDC */
		OUT_BCS_BATCH(batch,
					  quantization_value[i][3] << 16 | /* Y2AC */
					  quantization_value[i][2] <<  0); /* Y2DC */
	}

	/* CoeffProbability table for non-key frame, DW16-DW18 */
	if (probs_bo) {
		OUT_BCS_RELOC64(batch, probs_bo,
						0, I915_GEM_DOMAIN_INSTRUCTION,
						0);
		OUT_BCS_BATCH(batch, i965->intel.mocs_state);
	} else {
		OUT_BCS_BATCH(batch, 0);
		OUT_BCS_BATCH(batch, 0);
		OUT_BCS_BATCH(batch, 0);
	}

	OUT_BCS_BATCH(batch,
				  pic_param->mb_segment_tree_probs[2] << 16 |
				  pic_param->mb_segment_tree_probs[1] <<  8 |
				  pic_param->mb_segment_tree_probs[0] <<  0);

	OUT_BCS_BATCH(batch,
				  pic_param->prob_skip_false << 24 |
				  pic_param->prob_intra      << 16 |
				  pic_param->prob_last       <<  8 |
				  pic_param->prob_gf         <<  0);

	OUT_BCS_BATCH(batch,
				  pic_param->y_mode_probs[3] << 24 |
				  pic_param->y_mode_probs[2] << 16 |
				  pic_param->y_mode_probs[1] <<  8 |
				  pic_param->y_mode_probs[0] <<  0);

	OUT_BCS_BATCH(batch,
				  pic_param->uv_mode_probs[2] << 16 |
				  pic_param->uv_mode_probs[1] <<  8 |
				  pic_param->uv_mode_probs[0] <<  0);

	/* MV update value, DW23-DW32 */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 20; j += 4) {
			OUT_BCS_BATCH(batch,
						  (j + 3 == 19 ? 0 : pic_param->mv_probs[i][j + 3]) << 24 |
						  pic_param->mv_probs[i][j + 2] << 16 |
						  pic_param->mv_probs[i][j + 1] <<  8 |
						  pic_param->mv_probs[i][j + 0] <<  0);
		}
	}

	OUT_BCS_BATCH(batch,
				  (pic_param->loop_filter_deltas_ref_frame[3] & 0x7f) << 24 |
				  (pic_param->loop_filter_deltas_ref_frame[2] & 0x7f) << 16 |
				  (pic_param->loop_filter_deltas_ref_frame[1] & 0x7f) <<  8 |
				  (pic_param->loop_filter_deltas_ref_frame[0] & 0x7f) <<  0);

	OUT_BCS_BATCH(batch,
				  (pic_param->loop_filter_deltas_mode[3] & 0x7f) << 24 |
				  (pic_param->loop_filter_deltas_mode[2] & 0x7f) << 16 |
				  (pic_param->loop_filter_deltas_mode[1] & 0x7f) <<  8 |
				  (pic_param->loop_filter_deltas_mode[0] & 0x7f) <<  0);

	/* segmentation id stream base address, DW35-DW37 */
	if (enable_segmentation) {
		OUT_BCS_RELOC64(batch, gen7_mfd_context->segmentation_buffer.bo,
						0, I915_GEM_DOMAIN_INSTRUCTION,
						0);
		OUT_BCS_BATCH(batch, i965->intel.mocs_state);
	} else {
		OUT_BCS_BATCH(batch, 0);
		OUT_BCS_BATCH(batch, 0);
		OUT_BCS_BATCH(batch, 0);
	}
	ADVANCE_BCS_BATCH(batch);
}

static void
gen8_mfd_vp8_bsd_object(VADriverContextP ctx,
						VAPictureParameterBufferVP8 *pic_param,
						VASliceParameterBufferVP8 *slice_param,
						dri_bo *slice_data_bo,
						struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	int i, log2num;
	unsigned int offset = slice_param->slice_data_offset + ((slice_param->macroblock_offset + 7) >> 3);
	unsigned int used_bits = 8 - pic_param->bool_coder_ctx.count;
	unsigned int partition_size_0 = slice_param->partition_size[0];

	assert(pic_param->bool_coder_ctx.count >= 0 && pic_param->bool_coder_ctx.count <= 7);
	if (used_bits == 8) {
		used_bits = 0;
		offset += 1;
		partition_size_0 -= 1;
	}

	assert(slice_param->num_of_partitions >= 2);
	assert(slice_param->num_of_partitions <= 9);

	log2num = (int)log2(slice_param->num_of_partitions - 1);

	BEGIN_BCS_BATCH(batch, 22);
	OUT_BCS_BATCH(batch, MFD_VP8_BSD_OBJECT | (22 - 2));
	OUT_BCS_BATCH(batch,
				  used_bits << 16 | /* Partition 0 CPBAC Entropy Count */
				  pic_param->bool_coder_ctx.range <<  8 | /* Partition 0 Count Entropy Range */
				  log2num << 4 |
				  (slice_param->macroblock_offset & 0x7));
	OUT_BCS_BATCH(batch,
				  pic_param->bool_coder_ctx.value << 24 | /* Partition 0 Count Entropy Value */
				  0);

	OUT_BCS_BATCH(batch, partition_size_0 + 1);
	OUT_BCS_BATCH(batch, offset);
	//partion sizes in bytes are present after the above first partition when there are more than one token partition
	offset += (partition_size_0 + 3 * (slice_param->num_of_partitions - 2));
	for (i = 1; i < 9; i++) {
		if (i < slice_param->num_of_partitions) {
			OUT_BCS_BATCH(batch, slice_param->partition_size[i] + 1);
			OUT_BCS_BATCH(batch, offset);
		} else {
			OUT_BCS_BATCH(batch, 0);
			OUT_BCS_BATCH(batch, 0);
		}

		offset += slice_param->partition_size[i];
	}

	OUT_BCS_BATCH(batch, 0); /* concealment method */

	ADVANCE_BCS_BATCH(batch);
}

void
gen8_mfd_vp8_decode_picture(VADriverContextP ctx,
							struct decode_state *decode_state,
							struct gen7_mfd_context *gen7_mfd_context)
{
	struct intel_batchbuffer *batch = gen7_mfd_context->base.batch;
	VAPictureParameterBufferVP8 *pic_param;
	VASliceParameterBufferVP8 *slice_param;
	dri_bo *slice_data_bo;

	assert(decode_state->pic_param && decode_state->pic_param->buffer);
	pic_param = (VAPictureParameterBufferVP8 *)decode_state->pic_param->buffer;

	/* one slice per frame */
	if (decode_state->num_slice_params != 1 ||
		(!decode_state->slice_params ||
		 !decode_state->slice_params[0] ||
		 (decode_state->slice_params[0]->num_elements != 1 || decode_state->slice_params[0]->buffer == NULL)) ||
		(!decode_state->slice_datas ||
		 !decode_state->slice_datas[0] ||
		 !decode_state->slice_datas[0]->bo) ||
		!decode_state->probability_data) {
		WARN_ONCE("Wrong parameters for VP8 decoding\n");

		return;
	}

	slice_param = (VASliceParameterBufferVP8 *)decode_state->slice_params[0]->buffer;
	slice_data_bo = decode_state->slice_datas[0]->bo;

	gen8_mfd_vp8_decode_init(ctx, decode_state, gen7_mfd_context);
	intel_batchbuffer_start_atomic_bcs(batch, 0x1000);
	intel_batchbuffer_emit_mi_flush(batch);
	gen8_mfd_pipe_mode_select(ctx, decode_state, MFX_FORMAT_VP8, gen7_mfd_context);
	gen8_mfd_surface_state(ctx, decode_state, MFX_FORMAT_VP8, gen7_mfd_context);
	gen8_mfd_pipe_buf_addr_state(ctx, decode_state, MFX_FORMAT_VP8, gen7_mfd_context);
	gen8_mfd_bsp_buf_base_addr_state(ctx, decode_state, MFX_FORMAT_VP8, gen7_mfd_context);
	gen8_mfd_ind_obj_base_addr_state(ctx, slice_data_bo, MFX_FORMAT_VP8, gen7_mfd_context);
	gen8_mfd_vp8_pic_state(ctx, decode_state, gen7_mfd_context);
	gen8_mfd_vp8_bsd_object(ctx, pic_param, slice_param, slice_data_bo, gen7_mfd_context);
	intel_batchbuffer_end_atomic(batch);
	intel_batchbuffer_flush(batch);
}

static VAStatus
gen8_mfd_decode_picture(VADriverContextP ctx,
						VAProfile profile,
						union codec_state *codec_state,
						struct hw_context *hw_context)

{
	struct gen7_mfd_context *gen7_mfd_context = (struct gen7_mfd_context *)hw_context;
	struct decode_state *decode_state = &codec_state->decode;
	VAStatus vaStatus;

	assert(gen7_mfd_context);

	vaStatus = intel_decoder_sanity_check_input(ctx, profile, decode_state);

	if (vaStatus != VA_STATUS_SUCCESS)
		goto out;

	gen7_mfd_context->wa_mpeg2_slice_vertical_position = -1;

	switch (profile) {
	case VAProfileMPEG2Simple:
	case VAProfileMPEG2Main:
		gen8_mfd_mpeg2_decode_picture(ctx, decode_state, gen7_mfd_context);
		break;

	case VAProfileH264ConstrainedBaseline:
	case VAProfileH264Main:
	case VAProfileH264High:
	case VAProfileH264StereoHigh:
	case VAProfileH264MultiviewHigh:
		gen8_mfd_avc_decode_picture(ctx, decode_state, gen7_mfd_context);
		break;

	case VAProfileVC1Simple:
	case VAProfileVC1Main:
	case VAProfileVC1Advanced:
		gen8_mfd_vc1_decode_picture(ctx, decode_state, gen7_mfd_context);
		break;

	case VAProfileJPEGBaseline:
		gen8_mfd_jpeg_decode_picture(ctx, decode_state, gen7_mfd_context);
		break;

	case VAProfileVP8Version0_3:
		gen8_mfd_vp8_decode_picture(ctx, decode_state, gen7_mfd_context);
		break;

	default:
		assert(0);
		break;
	}

	vaStatus = VA_STATUS_SUCCESS;

out:
	return vaStatus;
}

static void
gen8_mfd_context_destroy(void *hw_context)
{
	VADriverContextP ctx;
	struct gen7_mfd_context *gen7_mfd_context = (struct gen7_mfd_context *)hw_context;

	ctx = (VADriverContextP)(gen7_mfd_context->driver_context);

	dri_bo_unreference(gen7_mfd_context->post_deblocking_output.bo);
	gen7_mfd_context->post_deblocking_output.bo = NULL;

	dri_bo_unreference(gen7_mfd_context->pre_deblocking_output.bo);
	gen7_mfd_context->pre_deblocking_output.bo = NULL;

	dri_bo_unreference(gen7_mfd_context->intra_row_store_scratch_buffer.bo);
	gen7_mfd_context->intra_row_store_scratch_buffer.bo = NULL;

	dri_bo_unreference(gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.bo);
	gen7_mfd_context->deblocking_filter_row_store_scratch_buffer.bo = NULL;

	dri_bo_unreference(gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.bo);
	gen7_mfd_context->bsd_mpc_row_store_scratch_buffer.bo = NULL;

	dri_bo_unreference(gen7_mfd_context->mpr_row_store_scratch_buffer.bo);
	gen7_mfd_context->mpr_row_store_scratch_buffer.bo = NULL;

	dri_bo_unreference(gen7_mfd_context->bitplane_read_buffer.bo);
	gen7_mfd_context->bitplane_read_buffer.bo = NULL;

	dri_bo_unreference(gen7_mfd_context->segmentation_buffer.bo);
	gen7_mfd_context->segmentation_buffer.bo = NULL;

	dri_bo_unreference(gen7_mfd_context->jpeg_wa_slice_data_bo);

	if (gen7_mfd_context->jpeg_wa_surface_id != VA_INVALID_SURFACE) {
		i965_DestroySurfaces(ctx,
							 &gen7_mfd_context->jpeg_wa_surface_id,
							 1);
		gen7_mfd_context->jpeg_wa_surface_object = NULL;
	}

	intel_batchbuffer_free(gen7_mfd_context->base.batch);
	free(gen7_mfd_context);
}

static void gen8_mfd_mpeg2_context_init(VADriverContextP ctx,
										struct gen7_mfd_context *gen7_mfd_context)
{
	gen7_mfd_context->iq_matrix.mpeg2.load_intra_quantiser_matrix = -1;
	gen7_mfd_context->iq_matrix.mpeg2.load_non_intra_quantiser_matrix = -1;
	gen7_mfd_context->iq_matrix.mpeg2.load_chroma_intra_quantiser_matrix = -1;
	gen7_mfd_context->iq_matrix.mpeg2.load_chroma_non_intra_quantiser_matrix = -1;
}

struct hw_context *
gen8_dec_hw_context_init(VADriverContextP ctx, struct object_config *obj_config)
{
	struct intel_driver_data *intel = intel_driver_data(ctx);
	struct gen7_mfd_context *gen7_mfd_context = calloc(1, sizeof(struct gen7_mfd_context));
	int i;

	if (!gen7_mfd_context)
		return NULL;

	gen7_mfd_context->base.destroy = gen8_mfd_context_destroy;
	gen7_mfd_context->base.run = gen8_mfd_decode_picture;
	gen7_mfd_context->base.batch = intel_batchbuffer_new(intel, I915_EXEC_RENDER, 0);

	for (i = 0; i < ARRAY_ELEMS(gen7_mfd_context->reference_surface); i++) {
		gen7_mfd_context->reference_surface[i].surface_id = VA_INVALID_ID;
		gen7_mfd_context->reference_surface[i].frame_store_id = -1;
	}

	gen7_mfd_context->jpeg_wa_surface_id = VA_INVALID_SURFACE;
	gen7_mfd_context->segmentation_buffer.valid = 0;

	/* Must be in LONG mode by default. */
	gen7_mfd_context->decoder_format_mode = MFX_LONG_MODE;

	switch (obj_config->profile) {
	case VAProfileMPEG2Simple:
	case VAProfileMPEG2Main:
		gen8_mfd_mpeg2_context_init(ctx, gen7_mfd_context);
		break;

	case VAProfileH264ConstrainedBaseline:
	case VAProfileH264Main:
	case VAProfileH264High:
	case VAProfileH264StereoHigh:
	case VAProfileH264MultiviewHigh:
		gen8_mfd_avc_context_init(ctx, obj_config, gen7_mfd_context);
		break;
	default:
		break;
	}

	gen7_mfd_context->driver_context = ctx;
	return (struct hw_context *)gen7_mfd_context;
}

static inline void gen8_get_hw_dec_formats(VADriverContextP ctx, struct object_config *obj_config,
	struct i965_driver_data* data, int *i, VASurfaceAttrib *attribs)
{
	switch (obj_config->profile)
	{
		case VAProfileJPEGBaseline:
		{
			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_IMC3;
			(*i)++;

			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_IMC1;
			(*i)++;

			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_Y800;
			(*i)++;

			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_411P;
			(*i)++;

			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_422H;
			(*i)++;

			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_422V;
			(*i)++;

			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_444P;
			(*i)++;
			
			break;
		}

		case VAProfileHEVCMain10:
		case VAProfileVP9Profile2:
		{
			if (!data->codec_info->has_vpp_p010)
				break;

			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_P010;
			(*i)++;

			FALLTHROUGH;
		}

		default:
		{
			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_NV12;
			(*i)++;

			break;
		}
	}
}

static inline void gen8_get_hw_enc_formats(VADriverContextP ctx, struct object_config *obj_config,
	struct i965_driver_data* data, int *i, VASurfaceAttrib *attribs)
{
	switch (obj_config->profile)
	{
		case VAProfileHEVCMain10:
		{
			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_P010;
			(*i)++;

			break;
		}

		case VAProfileJPEGBaseline:
		{
			if (obj_config->entrypoint == VAEntrypointEncPicture)
			{
				attribs[*i].type = VASurfaceAttribPixelFormat;
				attribs[*i].value.type = VAGenericValueTypeInteger;
				attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
				attribs[*i].value.value.i = VA_FOURCC_YUY2;
				(*i)++;

				attribs[*i].type = VASurfaceAttribPixelFormat;
				attribs[*i].value.type = VAGenericValueTypeInteger;
				attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
				attribs[*i].value.value.i = VA_FOURCC_UYVY;
				(*i)++;

				attribs[*i].type = VASurfaceAttribPixelFormat;
				attribs[*i].value.type = VAGenericValueTypeInteger;
				attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
				attribs[*i].value.value.i = VA_FOURCC_YV16;
				(*i)++;

				attribs[*i].type = VASurfaceAttribPixelFormat;
				attribs[*i].value.type = VAGenericValueTypeInteger;
				attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
				attribs[*i].value.value.i = VA_FOURCC_Y800;
				(*i)++;
			}

			FALLTHROUGH;
		}

		default:
		{
			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_NV12;
			(*i)++;

			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_I420;
			(*i)++;

			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_YV12;
			(*i)++;

			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_IMC3;
			(*i)++;
		}
	}
}

static inline void gen8_get_hw_vpp_formats(VADriverContextP ctx, struct object_config *obj_config,
	struct i965_driver_data* data, int *i, VASurfaceAttrib *attribs)
{
	attribs[*i].type = VASurfaceAttribPixelFormat;
	attribs[*i].value.type = VAGenericValueTypeInteger;
	attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
	attribs[*i].value.value.i = VA_FOURCC_NV12;
	(*i)++;

	attribs[*i].type = VASurfaceAttribPixelFormat;
	attribs[*i].value.type = VAGenericValueTypeInteger;
	attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
	attribs[*i].value.value.i = VA_FOURCC_I420;
	(*i)++;

	attribs[*i].type = VASurfaceAttribPixelFormat;
	attribs[*i].value.type = VAGenericValueTypeInteger;
	attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
	attribs[*i].value.value.i = VA_FOURCC_YV12;
	(*i)++;

	attribs[*i].type = VASurfaceAttribPixelFormat;
	attribs[*i].value.type = VAGenericValueTypeInteger;
	attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
	attribs[*i].value.value.i = VA_FOURCC_IMC3;
	(*i)++;

	attribs[*i].type = VASurfaceAttribPixelFormat;
	attribs[*i].value.type = VAGenericValueTypeInteger;
	attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
	attribs[*i].value.value.i = VA_FOURCC_RGBA;
	(*i)++;

	attribs[*i].type = VASurfaceAttribPixelFormat;
	attribs[*i].value.type = VAGenericValueTypeInteger;
	attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
	attribs[*i].value.value.i = VA_FOURCC_RGBX;
	(*i)++;

	attribs[*i].type = VASurfaceAttribPixelFormat;
	attribs[*i].value.type = VAGenericValueTypeInteger;
	attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
	attribs[*i].value.value.i = VA_FOURCC_BGRA;
	(*i)++;

	attribs[*i].type = VASurfaceAttribPixelFormat;
	attribs[*i].value.type = VAGenericValueTypeInteger;
	attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
	attribs[*i].value.value.i = VA_FOURCC_BGRX;
	(*i)++;

	attribs[*i].type = VASurfaceAttribPixelFormat;
	attribs[*i].value.type = VAGenericValueTypeInteger;
	attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
	attribs[*i].value.value.i = VA_FOURCC_ARGB;
	(*i)++;

	attribs[*i].type = VASurfaceAttribPixelFormat;
	attribs[*i].value.type = VAGenericValueTypeInteger;
	attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
	attribs[*i].value.value.i = VA_FOURCC_YUY2;
	(*i)++;

	attribs[*i].type = VASurfaceAttribPixelFormat;
	attribs[*i].value.type = VAGenericValueTypeInteger;
	attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
	attribs[*i].value.value.i = VA_FOURCC_UYVY;
	(*i)++;

	attribs[*i].type = VASurfaceAttribPixelFormat;
	attribs[*i].value.type = VAGenericValueTypeInteger;
	attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
	attribs[*i].value.value.i = VA_FOURCC_YV16;
	(*i)++;

	/**
	 * KBL/GLK or newer.
	 */
	if (data->codec_info->has_vpp_p010)
	{
		attribs[*i].type = VASurfaceAttribPixelFormat;
		attribs[*i].value.type = VAGenericValueTypeInteger;
		attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
		attribs[*i].value.value.i = VA_FOURCC_P010;
		(*i)++;

		attribs[*i].type = VASurfaceAttribPixelFormat;
		attribs[*i].value.type = VAGenericValueTypeInteger;
		attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
		attribs[*i].value.value.i = VA_FOURCC_I010;
		(*i)++;
	}
}

void gen8_get_hw_formats(VADriverContextP ctx, struct object_config *obj_config,
	struct i965_driver_data* data, int *i, VASurfaceAttrib *attribs)
{
	switch (obj_config->entrypoint)
	{
		case VAEntrypointVLD:
		{
			gen8_get_hw_dec_formats(ctx, obj_config, data, i, attribs);
			break;
		}

		case VAEntrypointEncSlice:
		case VAEntrypointEncSliceLP:
		case VAEntrypointEncPicture:
		case VAEntrypointFEI:
		{
			gen8_get_hw_enc_formats(ctx, obj_config, data, i, attribs);
			break;
		}

		case VAEntrypointVideoProc:
		{
			gen8_get_hw_vpp_formats(ctx, obj_config, data, i, attribs);
			break;			
		}

		case VAEntrypointStats:
		{
			attribs[*i].type = VASurfaceAttribPixelFormat;
			attribs[*i].value.type = VAGenericValueTypeInteger;
			attribs[*i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
			attribs[*i].value.value.i = VA_FOURCC_NV12;
			(*i)++;

			break;
		}

		default:
		{
			i965_log_debug(ctx, "gen8_get_hw_formats: Ignoring unknown entrypoint %#010x\n", obj_config->entrypoint);
			break;
		}
	}
}