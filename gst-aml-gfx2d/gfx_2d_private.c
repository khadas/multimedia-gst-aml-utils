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
#include "aml_ge2d.h"

#include "gfx_2d.h"
#include "gfx_2d_private.h"


/*************************************************
Description:    convert video rotation type
Input:
  rotation : Gfx define rotation enum
Return:         GE2D define rotation enum
*************************************************/
int gfx_convert_video_rotation(GfxAmlRotation rotation) {
  int ret = GE2D_ROTATION_0;

  switch (rotation) {
  case GFX_AML_ROTATION_90:
    ret = GE2D_ROTATION_90;
    break;
  case GFX_AML_ROTATION_180:
    ret = GE2D_ROTATION_180;
    break;
  case GFX_AML_ROTATION_270:
    ret = GE2D_ROTATION_270;
    break;
  case GFX_AML_MIRROR_X:
    ret = GE2D_MIRROR_X;
    break;
  case GFX_AML_MIRROR_Y:
    ret = GE2D_MIRROR_Y;
    break;
  default:
    ret = GE2D_ROTATION_0;
    break;
  }
  return ret;
}



void gfx_fill_params(GFX_Buf *pBuf,
                        GFX_Rect *pRect,
                        buffer_info_t *info) {
  info->plane_number = pBuf->plane_number;
  for (int i=0; i<info->plane_number; i++)
  {
    info->shared_fd[i] = pBuf->fd[i];
  }

  info->canvas_w = pBuf->size.w;
  info->canvas_h = pBuf->size.h;
  info->rect.x = pRect->x;
  info->rect.y = pRect->y;
  info->rect.w = pRect->w;
  info->rect.h = pRect->h;
  info->format = pBuf->format;
  info->memtype = GE2D_CANVAS_ALLOC;
  info->mem_alloc_type = AML_GE2D_MEM_DMABUF;
}


void gfx_clear_ge2d_info(aml_ge2d_info_t *pge2dinfo) {
    if (NULL == pge2dinfo) {
      return;
    }

    memset(&(pge2dinfo->src_info[0]), 0, sizeof(buffer_info_t));
    memset(&(pge2dinfo->src_info[1]), 0, sizeof(buffer_info_t));
    memset(&(pge2dinfo->dst_info), 0, sizeof(buffer_info_t));

    pge2dinfo->src_info[0].shared_fd[0] = -1;
    pge2dinfo->src_info[1].shared_fd[0] = -1;
    pge2dinfo->dst_info.shared_fd[0]    = -1;
}



GFX_Return gfx_check_buf_rect(GFX_Buf *pBuf, GFX_Rect *pRect) {
  if (pRect->x >= pBuf->size.w || pRect->y >= pBuf->size.h)
  {
    GFX_ERROR("invalid parameters,pBuf(fd:%d format:%d plane_number:%d size:(%d %d)) pRect(%d %d %d %d)",
      pBuf->fd[0], pBuf->format, pBuf->plane_number, pBuf->size.w, pBuf->size.h,
      pRect->x, pRect->y, pRect->w, pRect->h);
    return GFX_Ret_Error;
  }

  // if extend right border, align to right border
  if (pRect->x+pRect->w > pBuf->size.w) {
    pRect->w = pBuf->size.w-pRect->x;
  }

  // if extend bottom border, align to bottom border
  if (pRect->y+pRect->h > pBuf->size.h) {
    pRect->h = pBuf->size.h-pRect->y;
  }

  return GFX_Ret_OK;
}



void gfx_print_params(aml_ge2d_info_t *pge2dinfo) {
#if 1
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



GFX_Return gfx_do_ge2d_cmd(aml_ge2d_info_t *pge2dinfo, int sync) {
  if (NULL == pge2dinfo) return GFX_Ret_Error;

# if !GFX_SUPPORT_CMD_Q
  sync=1;
#endif

  if (-1 != pge2dinfo->src_info[0].shared_fd[0]) {
    if (pge2dinfo->src_info[0].mem_alloc_type == AML_GE2D_MEM_DMABUF) {
      aml_ge2d_sync_for_device(pge2dinfo, 0);
    }
  }
  if (-1 != pge2dinfo->src_info[1].shared_fd[0]) {
    if (pge2dinfo->src_info[1].mem_alloc_type == AML_GE2D_MEM_DMABUF) {
      aml_ge2d_sync_for_device(pge2dinfo, 0);
    }
  }

  if (1 == sync)  {
    if (aml_ge2d_process(pge2dinfo) != ge2d_success) {
      GFX_ERROR("aml_ge2d_process_enqueue failed");
      return GFX_Ret_Error;
    }
  }
  else  {
    if (aml_ge2d_process_enqueue(pge2dinfo) != ge2d_success) {
      GFX_ERROR("aml_ge2d_process_enqueue failed");
      return GFX_Ret_Error;
    }
  }

  if (pge2dinfo->dst_info.mem_alloc_type == AML_GE2D_MEM_DMABUF) {
    aml_ge2d_sync_for_cpu(pge2dinfo);
  }

  return GFX_Ret_OK;
}


