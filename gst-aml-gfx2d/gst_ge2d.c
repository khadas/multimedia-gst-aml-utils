/*
 * Copyright (C) 2014-2023 Amlogic, Inc. All rights reserved.
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
#include <stdlib.h>

#include <aml_ge2d.h>
#include "gfx_2d.h"
#include "gst_ge2d.h"

  // The following Ge2D color not confirmed
  // PIXEL_FORMAT_Y8                 = 7,           // YYYY
  // PIXEL_FORMAT_YU12,                             // YCbCr 4:2:0 Planar  YYYY......  U......V......
  // PIXEL_FORMAT_ARGB_1555,
  // PIXEL_FORMAT_ARGB_4444,
  // PIXEL_FORMAT_RGBA_4444,
  // PIXEL_FORMAT_CLUT8
  // GST_VIDEO_FORMAT_BGR16,

int gfx_convert_video_format(GstVideoFormat format) {
  int ret = -1;
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



