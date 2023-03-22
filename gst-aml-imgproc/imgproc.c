/*
 * Copyright (C) 2014-2019 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <gst/gst.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "imgproc.h"

#include "aml_ge2d.h"

struct frinfo {
  aml_ge2d_t ge2d;
};

  // The following Ge2D color not confirmed
  // PIXEL_FORMAT_Y8                 = 7,           // YYYY
  // PIXEL_FORMAT_YU12,                             // YCbCr 4:2:0 Planar  YYYY......  U......V......
  // PIXEL_FORMAT_ARGB_1555,
  // PIXEL_FORMAT_ARGB_4444,
  // PIXEL_FORMAT_RGBA_4444,
  // PIXEL_FORMAT_CLUT8
  // GST_VIDEO_FORMAT_BGR16,

gint convert_video_format(GstVideoFormat format) {
  gint ret = -1;
  switch (format) {
    case GST_VIDEO_FORMAT_RGB:
      ret = PIXEL_FORMAT_RGB_888;
      break;
    case GST_VIDEO_FORMAT_RGBA:
      ret = PIXEL_FORMAT_RGBA_8888;
      break;
    case GST_VIDEO_FORMAT_RGBx:
      ret = PIXEL_FORMAT_RGBX_8888;
      break;
    case GST_VIDEO_FORMAT_BGR:
      ret = PIXEL_FORMAT_BGR_888;
      break;
    case GST_VIDEO_FORMAT_BGRA:
      ret = PIXEL_FORMAT_BGRA_8888;
      break;
    case GST_VIDEO_FORMAT_ARGB:  // Add for ARGB8888
      ret = PIXEL_FORMAT_ARGB_8888;
      break;
    case GST_VIDEO_FORMAT_ABGR:  // Add for ABGR8888
      ret = PIXEL_FORMAT_ABGR_8888;
      break;
    case GST_VIDEO_FORMAT_GRAY8:  // Add for Gray
      ret = PIXEL_FORMAT_Y8;
      break;
    case GST_VIDEO_FORMAT_RGB16:  // Add for RGB565
      ret = PIXEL_FORMAT_RGB_565;
      break;
    case GST_VIDEO_FORMAT_YV12:
      ret = PIXEL_FORMAT_YV12;
      break;
    case GST_VIDEO_FORMAT_UYVY:
      ret = PIXEL_FORMAT_YCbCr_422_UYVY;
      break;
    case GST_VIDEO_FORMAT_NV21:
      ret = PIXEL_FORMAT_YCrCb_420_SP;
      break;
    case GST_VIDEO_FORMAT_NV16:
      ret = PIXEL_FORMAT_YCbCr_422_SP;
      break;
    case GST_VIDEO_FORMAT_NV12:
      ret = PIXEL_FORMAT_YCbCr_420_SP_NV12;
      break;
    case GST_VIDEO_FORMAT_I420:
      // used by vconv, treat as NV12
      ret = PIXEL_FORMAT_YCbCr_420_SP_NV12;
      break;
    default:
      break;
  }
  return ret;
}






gint convert_video_rotation(GstAmlRotation rotation) {
  gint ret;
  switch (rotation) {
  case GST_AML_ROTATION_90:
    ret = GE2D_ROTATION_90;
    break;
  case GST_AML_ROTATION_180:
    ret = GE2D_ROTATION_180;
    break;
  case GST_AML_ROTATION_270:
    ret = GE2D_ROTATION_270;
    break;
  case GST_AML_MIRROR_X:
    ret = GE2D_MIRROR_X;
    break;
  case GST_AML_MIRROR_Y:
    ret = GE2D_MIRROR_Y;
    break;
  default:
    ret = GE2D_ROTATION_0;
    break;
  }
  return ret;
}



static void gfx_print_params(aml_ge2d_info_t *pge2dinfo) {
#if 0
  GFX_INFO("gfx_print_params, src_info[0] canvas(%d %d) "\
    "rect(%d %d %d %d) shared_fd=%d mem_alloc_type=%d "\
    "format=%d memtype=%d layer_mode=%d plane_alpha=%d plane_number=%d",
    pge2dinfo->src_info[0].canvas_w,
    pge2dinfo->src_info[0].canvas_h,
    pge2dinfo->src_info[0].rect.x,
    pge2dinfo->src_info[0].rect.y,
    pge2dinfo->src_info[0].rect.w,
    pge2dinfo->src_info[0].rect.h,
    pge2dinfo->src_info[0].shared_fd[0],
    pge2dinfo->src_info[0].mem_alloc_type,
    pge2dinfo->src_info[0].format,
    pge2dinfo->src_info[0].memtype,
    pge2dinfo->src_info[0].layer_mode,
    pge2dinfo->src_info[0].plane_alpha,
    pge2dinfo->src_info[0].plane_number);

  GFX_INFO("gfx_print_params, src_info[1] canvas(%d %d) "\
    "rect(%d %d %d %d) shared_fd=%d mem_alloc_type=%d "\
    "format=%d memtype=%d layer_mode=%d plane_alpha=%d plane_number=%d",
    pge2dinfo->src_info[1].canvas_w,
    pge2dinfo->src_info[1].canvas_h,
    pge2dinfo->src_info[1].rect.x,
    pge2dinfo->src_info[1].rect.y,
    pge2dinfo->src_info[1].rect.w,
    pge2dinfo->src_info[1].rect.h,
    pge2dinfo->src_info[1].shared_fd[0],
    pge2dinfo->src_info[1].mem_alloc_type,
    pge2dinfo->src_info[1].format,
    pge2dinfo->src_info[1].memtype,
    pge2dinfo->src_info[1].layer_mode,
    pge2dinfo->src_info[1].plane_alpha,
    pge2dinfo->src_info[1].plane_number);

  GFX_INFO("gfx_print_params, dst_info canvas(%d %d) "\
    "rect(%d %d %d %d) shared_fd=%d mem_alloc_type=%d "\
    "format=%d memtype=%d layer_mode=%d plane_alpha=%d plane_number=%d rotation=%d",
    pge2dinfo->dst_info.canvas_w,
    pge2dinfo->dst_info.canvas_h,
    pge2dinfo->dst_info.rect.x,
    pge2dinfo->dst_info.rect.y,
    pge2dinfo->dst_info.rect.w,
    pge2dinfo->dst_info.rect.h,
    pge2dinfo->dst_info.shared_fd[0],
    pge2dinfo->dst_info.mem_alloc_type,
    pge2dinfo->dst_info.format,
    pge2dinfo->dst_info.memtype,
    pge2dinfo->dst_info.layer_mode,
    pge2dinfo->dst_info.plane_alpha,
    pge2dinfo->dst_info.plane_number,
    pge2dinfo->dst_info.rotation);

  GFX_INFO("gfx_print_params, offset=%d ge2d_op=%d "\
    "blend_mode=%d color=%x gl_alpha=%d const_color=%d",
    pge2dinfo->offset,
    pge2dinfo->ge2d_op,
    pge2dinfo->blend_mode,
    pge2dinfo->color,
    pge2dinfo->gl_alpha,
    pge2dinfo->const_color);
#endif
}



gboolean imgproc_transform(void *handle, struct imgproc_buf in_buf,
                           struct imgproc_pos in_pos, GstVideoFormat in_format,
                           struct imgproc_buf out_buf,
                           struct imgproc_pos out_pos,
                           GstVideoFormat out_format, GstAmlRotation rotation) {
  struct frinfo *f = (struct frinfo*) handle;
  if (f == NULL) return FALSE;

  GST_INFO("imgproc_transform start");

  aml_ge2d_info_t *pge2dinfo = &f->ge2d.ge2dinfo;

  pge2dinfo->src_info[0].canvas_w = in_pos.canvas_w;
  pge2dinfo->src_info[0].canvas_h = in_pos.canvas_h;
  pge2dinfo->src_info[0].rect.x = in_pos.x;
  pge2dinfo->src_info[0].rect.y = in_pos.y;
  pge2dinfo->src_info[0].rect.w = in_pos.w;
  pge2dinfo->src_info[0].rect.h = in_pos.h;
  pge2dinfo->src_info[0].shared_fd[0] = in_buf.fd;
  pge2dinfo->src_info[0].mem_alloc_type =
      in_buf.is_ionbuf ? AML_GE2D_MEM_ION : AML_GE2D_MEM_DMABUF;
  pge2dinfo->src_info[0].format = convert_video_format(in_format);
  pge2dinfo->src_info[0].memtype = GE2D_CANVAS_ALLOC;
  pge2dinfo->src_info[0].layer_mode = 0;
  pge2dinfo->src_info[0].plane_alpha = 0xff;
  pge2dinfo->src_info[0].plane_number = 1;

  pge2dinfo->src_info[1].canvas_w = 0;
  pge2dinfo->src_info[1].canvas_h = 0;
  pge2dinfo->src_info[1].format = -1;
  pge2dinfo->src_info[1].memtype = GE2D_CANVAS_TYPE_INVALID;
  pge2dinfo->src_info[1].shared_fd[0] = -1;
  pge2dinfo->src_info[1].mem_alloc_type = AML_GE2D_MEM_ION;
  pge2dinfo->src_info[1].plane_number = 1;

  pge2dinfo->dst_info.canvas_w = out_pos.canvas_w;
  pge2dinfo->dst_info.canvas_h = out_pos.canvas_h;
  pge2dinfo->dst_info.rect.x = out_pos.x;
  pge2dinfo->dst_info.rect.y = out_pos.y;
  pge2dinfo->dst_info.rect.w = out_pos.w;
  pge2dinfo->dst_info.rect.h = out_pos.h;
  pge2dinfo->dst_info.shared_fd[0] = out_buf.fd;
  pge2dinfo->dst_info.mem_alloc_type =
      out_buf.is_ionbuf ? AML_GE2D_MEM_ION : AML_GE2D_MEM_DMABUF;
  pge2dinfo->dst_info.format = convert_video_format(out_format);
  pge2dinfo->dst_info.rotation = convert_video_rotation(rotation);
  pge2dinfo->dst_info.memtype = GE2D_CANVAS_ALLOC;
  pge2dinfo->dst_info.plane_number = 1;

  pge2dinfo->offset = 0;
  pge2dinfo->ge2d_op = AML_GE2D_STRETCHBLIT;
  pge2dinfo->blend_mode = BLEND_MODE_NONE;
  pge2dinfo->blend_mode = BLEND_MODE_PREMULTIPLIED;

  pge2dinfo->color = 0;
  pge2dinfo->gl_alpha = 0;
  pge2dinfo->const_color = 0;

  gfx_print_params(pge2dinfo);

  if (aml_ge2d_process (pge2dinfo) < 0) {
    return FALSE;
  }

  return TRUE;
}

gboolean imgproc_crop(void *handle, struct imgproc_buf in_buf,
                      struct imgproc_pos in_pos, GstVideoFormat in_format,
                      struct imgproc_buf out_buf, struct imgproc_pos out_pos,
                      GstVideoFormat out_format) {
  GST_INFO("imgproc_crop start");

  return imgproc_transform(handle, in_buf, in_pos, in_format, out_buf, out_pos,
                           out_format, GST_AML_ROTATION_0);
}

gboolean imgproc_fillrect(void *handle, GstVideoFormat format,
                          struct imgproc_buf buf, struct imgproc_pos pos,
                          guint color) {
  struct frinfo *f = (struct frinfo *)handle;
  if (f == NULL) return FALSE;

  GST_INFO("imgproc_fillrect start");

  aml_ge2d_info_t *pge2dinfo = &f->ge2d.ge2dinfo;

  pge2dinfo->dst_info.shared_fd[0] = buf.fd;
  pge2dinfo->dst_info.mem_alloc_type =
      buf.is_ionbuf ? AML_GE2D_MEM_ION : AML_GE2D_MEM_DMABUF;
  pge2dinfo->dst_info.format = convert_video_format(format);
  pge2dinfo->dst_info.rect.x = pos.x;
  pge2dinfo->dst_info.rect.y = pos.y;
  pge2dinfo->dst_info.rect.w = pos.w;
  pge2dinfo->dst_info.rect.h = pos.h;
  pge2dinfo->dst_info.canvas_w = pos.canvas_w;
  pge2dinfo->dst_info.canvas_h = pos.canvas_h;
  pge2dinfo->dst_info.rotation = GE2D_ROTATION_0;
  pge2dinfo->dst_info.plane_number = 1;

  pge2dinfo->color = color;
  pge2dinfo->ge2d_op = AML_GE2D_FILLRECTANGLE;
  pge2dinfo->blend_mode = BLEND_MODE_PREMULTIPLIED;

  gfx_print_params(pge2dinfo);

  if (aml_ge2d_process (pge2dinfo) < 0) {
    return FALSE;
  }

  return TRUE;
}

void *imgproc_init() {
  struct frinfo *f = g_new0(struct frinfo, 1);

  int rc = aml_ge2d_init (&f->ge2d);
  if (rc != ge2d_success) {
    g_free (f);
    return NULL;
  }

  return (void *)f;

}

void imgproc_deinit (void *handle) {
  struct frinfo *f = (struct frinfo*) handle;
  if (f == NULL) return;

  aml_ge2d_exit (&f->ge2d);
  g_free (f);
  return;
}

