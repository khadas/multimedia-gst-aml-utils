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
#ifndef _GFX_2D_HW_PRIVATE_H
#define _GFX_2D_HW_PRIVATE_H

#include <gst/gst.h>


#define GFX_LOG_ENABLE 1

#define GFX_SUPPORT_CMD_Q 0


#if GFX_LOG_ENABLE
#define GFX_INFO GST_INFO
#else
#define GFX_INFO
#endif

#define GFX_ERROR GST_ERROR


typedef struct _gfx_info {
  aml_ge2d_t ge2d;
}gfx_info;



#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))



/*************************************************
Description:    convert video rotation type
Input:
  rotation : Gfx define rotation enum
Return:         GE2D define rotation enum
*************************************************/
int gfx_convert_video_rotation(GfxAmlRotation rotation);

void gfx_fill_params(GFX_Buf *pBuf,
                      GFX_Rect *pRect,
                      buffer_info_t *info);

void gfx_clear_ge2d_info(aml_ge2d_info_t *pge2dinfo);


GFX_Return gfx_check_buf_rect(GFX_Buf *pBuf, GFX_Rect *pRect);


void gfx_print_params(aml_ge2d_info_t *pge2dinfo);

GFX_Return gfx_do_ge2d_cmd(aml_ge2d_info_t *pInfo, int sync);


#endif /* _GFX_2D_HW_PRIVATE_H */
