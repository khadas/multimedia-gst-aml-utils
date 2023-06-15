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
#include <gst/gst.h>
#include "aml_ge2d.h"

#include "gfx_2d.h"
#include "gfx_2d_private.h"


GFX_Handle gfx_init() {
  GFX_INFO("Enter");

  gfx_info *pInfo = malloc(sizeof(gfx_info));
  if (NULL == pInfo)  {
    GFX_ERROR("malloc gfx_info error");
    return NULL;
  }

  if (aml_ge2d_init(&pInfo->ge2d) != ge2d_success) {
    g_free(pInfo);
    GFX_ERROR("aml_ge2d_init failed");
    return NULL;
  }
  return (GFX_Handle)pInfo;
}



GFX_Return gfx_deinit(GFX_Handle handle) {
  if (NULL == handle) return GFX_Ret_Error;
  gfx_info *pInfo = (gfx_info *)handle;

  GFX_INFO(" handle=%p", handle);

  aml_ge2d_exit(&pInfo->ge2d);

  free(pInfo);

  return GFX_Ret_OK;
}



GFX_Return gfx_sync_cmd(GFX_Handle handle) {
# if GFX_SUPPORT_CMD_Q
  if (NULL == handle) return GFX_Ret_Error;

  GFX_INFO("Enter handle=%p", handle);
  gfx_info *info = (gfx_info *)handle;
  aml_ge2d_info_t *pge2dinfo = &info->ge2d.ge2dinfo;

  if (aml_ge2d_post_queue(pge2dinfo) != ge2d_success) {
    GFX_ERROR("aml_ge2d_post_queue failed");
    return GFX_Ret_Error;
  }
#endif

  return GFX_Ret_OK;
}



GFX_Return gfx_fillrect(GFX_Handle handle,
                        GFX_Buf *pBuf,
                        GFX_Rect *pRect,
                        int color,
                        int sync) {
  if (NULL == handle) return GFX_Ret_Error;
  if (NULL == pBuf) return GFX_Ret_Error;
  if (NULL == pRect) return GFX_Ret_Error;
  gfx_info *info = (gfx_info *)handle;
  aml_ge2d_info_t *pge2dinfo = &info->ge2d.ge2dinfo;

  GFX_INFO("Enter handle=%p pBuf(fd:%d format:%d ion:%d size:(%d %d)) pRect(%d %d %d %d) color=%x",
    handle,
    pBuf->fd, pBuf->format, pBuf->is_ionbuf, pBuf->size.w, pBuf->size.h,
    pRect->x, pRect->y, pRect->w, pRect->h, color);

  if (GFX_Ret_OK != gfx_check_buf_rect(pBuf, pRect))  {
    return GFX_Ret_Error;
  }

  // clear ge2d structure
  gfx_clear_ge2d_info(pge2dinfo);

  // do fillrect
  pge2dinfo->src_info[0].canvas_w = pBuf->size.w;
  pge2dinfo->src_info[0].canvas_h = pBuf->size.h;
  pge2dinfo->src_info[0].format = pBuf->format;

  gfx_fill_params(pBuf, pRect, &pge2dinfo->dst_info);
  pge2dinfo->dst_info.layer_mode = LAYER_MODE_PREMULTIPLIED;
  pge2dinfo->dst_info.plane_alpha = GFX_DEFAULT_ALPHA;
  pge2dinfo->dst_info.memtype = GE2D_CANVAS_ALLOC;
  pge2dinfo->dst_info.plane_number = 1;

  pge2dinfo->color = color;
  pge2dinfo->ge2d_op = AML_GE2D_FILLRECTANGLE;
  pge2dinfo->blend_mode = BLEND_MODE_PREMULTIPLIED;

  gfx_print_params(pge2dinfo);
  if (gfx_do_ge2d_cmd(pge2dinfo, sync) < 0) {
    GFX_ERROR("gfx_do_ge2d_cmd failed");
    gfx_print_params(pge2dinfo);
    return GFX_Ret_Error;
  }

  return GFX_Ret_OK;
}



GFX_Return gfx_drawrect(GFX_Handle handle,
                        GFX_Buf *pBuf,
                        GFX_Rect *pRect,
                        int color,
                        int thickness,
                        int sync) {
  if (NULL == handle) return GFX_Ret_Error;
  if (NULL == pBuf) return GFX_Ret_Error;
  if (NULL == pRect) return GFX_Ret_Error;
  gfx_info *info = (gfx_info *)handle;
  aml_ge2d_info_t *pge2dinfo = &info->ge2d.ge2dinfo;

  GFX_INFO("Enter handle=%p pBuf(fd:%d format:%d ion:%d size:(%d %d)) pRect(%d %d %d %d) color=%x thickness=%x",
    handle,
    pBuf->fd, pBuf->format, pBuf->is_ionbuf, pBuf->size.w, pBuf->size.h,
    pRect->x, pRect->y, pRect->w, pRect->h, color, thickness);

  if (GFX_Ret_OK != gfx_check_buf_rect(pBuf, pRect))
  {
    return GFX_Ret_Error;
  }

  // too small rectangle, will cause memory overwrite
  if (pRect->w <= thickness || pRect->h <= thickness)
  {
    return GFX_Ret_OK;
  }

  // clear ge2d structure
  gfx_clear_ge2d_info(pge2dinfo);

  // do outer fillrect

  pge2dinfo->src_info[0].canvas_w = pBuf->size.w;
  pge2dinfo->src_info[0].canvas_h = pBuf->size.h;
  pge2dinfo->src_info[0].format = pBuf->format;

  gfx_fill_params(pBuf, pRect, &pge2dinfo->dst_info);
  pge2dinfo->dst_info.layer_mode = LAYER_MODE_PREMULTIPLIED;
  pge2dinfo->dst_info.plane_alpha = GFX_DEFAULT_ALPHA;
  pge2dinfo->dst_info.memtype = GE2D_CANVAS_ALLOC;
  pge2dinfo->dst_info.plane_number = 1;

  pge2dinfo->color = color;
  pge2dinfo->ge2d_op = AML_GE2D_FILLRECTANGLE;
  pge2dinfo->blend_mode = BLEND_MODE_PREMULTIPLIED;

  if (gfx_do_ge2d_cmd(pge2dinfo, sync) < 0) {
    GFX_ERROR("gfx_do_ge2d_cmd failed");
    gfx_print_params(pge2dinfo);
    return GFX_Ret_Error;
  }

  GFX_INFO("Enter outer rectangle done");

  // clear ge2d structure
  //gfx_clear_ge2d_info(pge2dinfo);

  // prepare for inner fillrect
  GFX_Rect inner_rect;
  inner_rect.x = pRect->x+thickness;
  inner_rect.y = pRect->y+thickness;
  inner_rect.w = pRect->w-thickness*2;
  inner_rect.h = pRect->h-thickness*2;

  // // do inner fillrect
  gfx_fill_params(pBuf, &inner_rect, &pge2dinfo->dst_info);
  pge2dinfo->dst_info.layer_mode = LAYER_MODE_PREMULTIPLIED;
  pge2dinfo->dst_info.plane_alpha = GFX_DEFAULT_ALPHA;
  pge2dinfo->dst_info.memtype = GE2D_CANVAS_ALLOC;
  pge2dinfo->dst_info.plane_number = 1;

  pge2dinfo->color = 0;
  pge2dinfo->ge2d_op = AML_GE2D_FILLRECTANGLE;
  pge2dinfo->blend_mode = BLEND_MODE_PREMULTIPLIED;

  if (gfx_do_ge2d_cmd(pge2dinfo, sync) < 0) {
    GFX_ERROR("gfx_do_ge2d_cmd failed");
    gfx_print_params(pge2dinfo);
    return GFX_Ret_Error;
  }

  return GFX_Ret_OK;
}



static void gfx_fillpixelbycolor(unsigned char *pMemory, int color)
{
  *(int *)pMemory=color;
}




GFX_Return gfx_fillrect_software(GFX_Handle handle,
                                  GFX_Buf *pBuf,
                                  unsigned char *pMemory,
                                  GFX_Rect *pRect,
                                  int color)
{
  if (NULL == handle) return GFX_Ret_Error;
  if (NULL == pBuf) return GFX_Ret_Error;
  if (NULL == pMemory) return GFX_Ret_Error;
  if (NULL == pRect) return GFX_Ret_Error;

  GFX_INFO("handle=%p pBuf(fd:%d format:%d ion:%d size:(%d %d)) pMemory(%p) pRect(%d %d %d %d) color=%x",
    handle,
    pBuf->fd, pBuf->format, pBuf->is_ionbuf, pBuf->size.w, pBuf->size.h, pMemory,
    pRect->x, pRect->y, pRect->w, pRect->h, color);

  if (GFX_Ret_OK != gfx_check_buf_rect(pBuf, pRect))
  {
    return GFX_Ret_Error;
  }

  int pitch = (((pBuf->size.w * 4) + 31) & ~31);
  int i = 0;
  int j = 0;

  int x = pRect->x;
  int y = pRect->y;
  int w = pRect->w;
  int h = pRect->h;

  // top
  for (i=y; i<y+h; i++)
  {
    for (j=x; j<x+w; j++)
    {
      gfx_fillpixelbycolor(&pMemory[i*pitch+j*4], color);
    }
  }

  return GFX_Ret_OK;
}



GFX_Return gfx_drawrect_software(GFX_Handle handle,
                                GFX_Buf *pBuf,
                                unsigned char *pMemory,
                                GFX_Rect *pRect,
                                int color,
                                int thickness)
{
  if (NULL == handle) return GFX_Ret_Error;
  if (NULL == pBuf) return GFX_Ret_Error;
  if (NULL == pMemory) return GFX_Ret_Error;
  if (NULL == pRect) return GFX_Ret_Error;

  GFX_INFO("handle=%p pBuf(fd:%d format:%d ion:%d size:(%d %d)) pMemory(%p) pRect(%d %d %d %d) color=%x thickness=%d",
    handle,
    pBuf->fd, pBuf->format, pBuf->is_ionbuf, pBuf->size.w, pBuf->size.h, pMemory,
    pRect->x, pRect->y, pRect->w, pRect->h, color, thickness);

  if (GFX_Ret_OK != gfx_check_buf_rect(pBuf, pRect))
  {
    return GFX_Ret_Error;
  }

  // too small rectangle, will cause memory overwrite
  if (pRect->w <= thickness || pRect->h <= thickness)
  {
    return GFX_Ret_OK;
  }

  int pitch = (((pBuf->size.w * 4) + 31) & ~31);
  int i = 0;
  int j = 0;

  int x = pRect->x;
  int y = pRect->y;
  int w = pRect->w;
  int h = pRect->h;

  // top
  for (i=y; i<y+thickness; i++)
  {
    for (j=x; j<x+w; j++)
    {
      gfx_fillpixelbycolor(&pMemory[i*pitch+j*4], color);
    }
  }

  // bottom
  for (i=y+h-thickness; i<y+h; i++)
  {
    for (j=x; j<x+w; j++)
    {
      gfx_fillpixelbycolor(&pMemory[i*pitch+j*4], color);
    }
  }

  // left
  for (i=y+thickness; i<y+h-thickness; i++)
  {
    for (j=x; j<x+thickness; j++)
    {
      gfx_fillpixelbycolor(&pMemory[i*pitch+j*4], color);
    }
  }

  // right
  for (i=y+thickness; i<y+h-thickness; i++)
  {
    for (j=x+w-thickness; j<x+w; j++)
    {
      gfx_fillpixelbycolor(&pMemory[i*pitch+j*4], color);
    }
  }

  return GFX_Ret_OK;
}



GFX_Return gfx_blend(GFX_Handle handle,
                     GFX_Buf *pBottomBuf,
                     GFX_Rect *pBottomRect,
                     GFX_Buf *pTopBuf,
                     GFX_Rect *pTopRect,
                     GFX_Buf *pOutBuf,
                     GFX_Rect *pOutRect,
                     unsigned char alpha,
                     int sync) {
  if (NULL == handle) return GFX_Ret_Error;
  if (NULL == pBottomBuf) return GFX_Ret_Error;
  if (NULL == pBottomRect) return GFX_Ret_Error;
  if (NULL == pTopBuf) return GFX_Ret_Error;
  if (NULL == pTopRect) return GFX_Ret_Error;
  if (NULL == pOutBuf) return GFX_Ret_Error;
  if (NULL == pOutRect) return GFX_Ret_Error;
  gfx_info *info = (gfx_info *)handle;
  aml_ge2d_info_t *pge2dinfo = &info->ge2d.ge2dinfo;

  GFX_INFO("Enter handle=%p "\
    "pBottomBuf(fd:%d format:%d ion:%d size:(%d %d)) pBottomRect(%d %d %d %d) "\
    "pTopBuf(fd:%d format:%d ion:%d size:(%d %d)) pTopRect(%d %d %d %d) "\
    "pOutBuf(fd:%d format:%d ion:%d size:(%d %d)) pOutRect(%d %d %d %d) alpha=%d",
    handle,
    pBottomBuf->fd, pBottomBuf->format, pBottomBuf->is_ionbuf, pBottomBuf->size.w, pBottomBuf->size.h,
    pBottomRect->x, pBottomRect->y, pBottomRect->w, pBottomRect->h,
    pTopBuf->fd, pTopBuf->format, pTopBuf->is_ionbuf, pTopBuf->size.w, pTopBuf->size.h,
    pTopRect->x, pTopRect->y, pTopRect->w, pTopRect->h,
    pOutBuf->fd, pOutBuf->format, pOutBuf->is_ionbuf, pOutBuf->size.w, pOutBuf->size.h,
    pOutRect->x, pOutRect->y, pOutRect->w, pOutRect->h, alpha);

  if (GFX_Ret_OK != gfx_check_buf_rect(pBottomBuf, pBottomRect))
  {
    return GFX_Ret_Error;
  }
  if (GFX_Ret_OK != gfx_check_buf_rect(pTopBuf, pTopRect))
  {
    return GFX_Ret_Error;
  }
  if (GFX_Ret_OK != gfx_check_buf_rect(pOutBuf, pOutRect))
  {
    return GFX_Ret_Error;
  }

  // clear ge2d structure
  gfx_clear_ge2d_info(pge2dinfo);

  // do blend
  // bottom buffer
  gfx_fill_params(pBottomBuf, pBottomRect, &pge2dinfo->src_info[0]);
  pge2dinfo->src_info[0].layer_mode = LAYER_MODE_PREMULTIPLIED;
  pge2dinfo->src_info[0].plane_alpha = 0xFF;
  pge2dinfo->src_info[0].plane_number = 1;
  pge2dinfo->src_info[0].memtype = GE2D_CANVAS_ALLOC;

  // top buffer
  gfx_fill_params(pTopBuf, pTopRect, &pge2dinfo->src_info[1]);
  pge2dinfo->src_info[1].layer_mode = LAYER_MODE_COVERAGE;
  pge2dinfo->src_info[1].plane_alpha = alpha;  // top view, blending
  pge2dinfo->src_info[1].plane_number = 1;
  pge2dinfo->src_info[1].memtype = GE2D_CANVAS_ALLOC;

  // destination buffer
  gfx_fill_params(pOutBuf, pOutRect, &pge2dinfo->dst_info);
  pge2dinfo->dst_info.memtype = GE2D_CANVAS_ALLOC;
  pge2dinfo->dst_info.plane_number = 1;

  pge2dinfo->ge2d_op = AML_GE2D_BLEND;
  pge2dinfo->blend_mode = BLEND_MODE_PREMULTIPLIED;

  if (gfx_do_ge2d_cmd(pge2dinfo, sync) < 0) {
    GFX_ERROR("gfx_do_ge2d_cmd failed");
    gfx_print_params(pge2dinfo);
    return GFX_Ret_Error;
  }

  return GFX_Ret_OK;
}


GFX_Return gfx_stretchblit(GFX_Handle handle,
                          GFX_Buf *pInBuf,
                          GFX_Rect *pInRect,
                          GFX_Buf *pOutBuf,
                          GFX_Rect *pOutRect,
                          GfxAmlRotation rotation,
                          int sync){
  if (NULL == handle) return GFX_Ret_Error;
  if (NULL == pInBuf) return GFX_Ret_Error;
  if (NULL == pInRect) return GFX_Ret_Error;
  if (NULL == pOutBuf) return GFX_Ret_Error;
  if (NULL == pOutRect) return GFX_Ret_Error;
  gfx_info *info = (gfx_info *)handle;
  aml_ge2d_info_t *pge2dinfo = &info->ge2d.ge2dinfo;

  GFX_INFO("Enter handle=%p "\
    "pInBuf(fd:%d format:%d ion:%d size:(%d %d)) pInRect(%d %d %d %d) "\
    "pOutBuf(fd:%d format:%d ion:%d size:(%d %d)) pOutRect(%d %d %d %d)",
    handle,
    pInBuf->fd, pInBuf->format, pInBuf->is_ionbuf, pInBuf->size.w, pInBuf->size.h,
    pInRect->x, pInRect->y, pInRect->w, pInRect->h,
    pOutBuf->fd, pOutBuf->format, pOutBuf->is_ionbuf, pOutBuf->size.w, pOutBuf->size.h,
    pOutRect->x, pOutRect->y, pOutRect->w, pOutRect->h);

  if (GFX_Ret_OK != gfx_check_buf_rect(pInBuf, pInRect))
  {
    return GFX_Ret_Error;
  }
  if (GFX_Ret_OK != gfx_check_buf_rect(pOutBuf, pOutRect))
  {
    return GFX_Ret_Error;
  }

  // clear ge2d structure
  gfx_clear_ge2d_info(pge2dinfo);

  // do stretchblit
  gfx_fill_params(pInBuf, pInRect, &pge2dinfo->src_info[0]);
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

  gfx_fill_params(pOutBuf, pOutRect, &pge2dinfo->dst_info);
  pge2dinfo->dst_info.rotation = gfx_convert_video_rotation(rotation);
  pge2dinfo->dst_info.memtype = GE2D_CANVAS_ALLOC;
  pge2dinfo->dst_info.plane_number = 1;

  pge2dinfo->offset = 0;
  pge2dinfo->ge2d_op = AML_GE2D_STRETCHBLIT;
  pge2dinfo->blend_mode = BLEND_MODE_PREMULTIPLIED;

  pge2dinfo->color = 0;
  pge2dinfo->gl_alpha = GFX_DEFAULT_ALPHA;
  pge2dinfo->const_color = 0;

  if (gfx_do_ge2d_cmd(pge2dinfo, sync) < 0) {
    GFX_ERROR("gfx_do_ge2d_cmd failed");
    gfx_print_params(pge2dinfo);
    return GFX_Ret_Error;
  }

  return GFX_Ret_OK;
}



GFX_Return gfx_updateDirtyArea(GFX_Buf *pBuf,
                                GFX_Rect *pBaseRect,
                                GFX_Rect *pNewRect){
  if (NULL == pBuf) return GFX_Ret_Error;
  if (NULL == pBaseRect) return GFX_Ret_Error;
  if (NULL == pNewRect) return GFX_Ret_Error;

  if (GFX_Ret_OK != gfx_check_buf_rect(pBuf, pBaseRect))
  {
    return GFX_Ret_Error;
  }
  if (GFX_Ret_OK != gfx_check_buf_rect(pBuf, pNewRect))
  {
    return GFX_Ret_Error;
  }

  // if base rect is zero rectangle, use new rect instead
  if (0 == pBaseRect->x && 0 == pBaseRect->y && 0 == pBaseRect->w && 0 == pBaseRect->h)
  {
    memcpy(pBaseRect, pNewRect, sizeof(GFX_Rect));
    return GFX_Ret_OK;
  }

  GFX_Rect rect = {0};

  // Union the base rect and new rect together

  rect.x = MIN(pBaseRect->x, pNewRect->x);
  rect.y = MIN(pBaseRect->y, pNewRect->y);

  int right = MAX(pBaseRect->x+pBaseRect->w, pNewRect->x+pNewRect->w);
  int bottom = MAX(pBaseRect->y+pBaseRect->h, pNewRect->y+pNewRect->h);

  rect.w = right-rect.x;
  rect.h = bottom-rect.y;

  memcpy(pBaseRect, &rect, sizeof(GFX_Rect));

  return GFX_Ret_OK;
}



int gfx_isEmptyArea(GFX_Rect *pRect){
  if (NULL == pRect) return 1;

  // if rect is zero rectangle, use new rect instead
  if (0 == pRect->x && 0 == pRect->y && 0 == pRect->w && 0 == pRect->h)
  {
    return 1;
  }

  return 0;
}

