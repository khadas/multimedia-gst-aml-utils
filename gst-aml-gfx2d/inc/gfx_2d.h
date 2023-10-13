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
#ifndef _GFX_2D_HW_H
#define _GFX_2D_HW_H



typedef struct _GFX_Rectangle
{
  unsigned int x;
  unsigned int y;
  unsigned int w;
  unsigned int h;
}GFX_Rect;


typedef struct _GFX_Position
{
  unsigned int x;
  unsigned int y;
}GFX_Pos;


typedef struct _GFX_Size
{
  unsigned int w;
  unsigned int h;
}GFX_Size;

#define MAX_PLANE_NUM 4

typedef struct _GFX_Buffer {
  unsigned int format;   // ge2d color format
  unsigned int plane_number;
  unsigned int fd[MAX_PLANE_NUM];
  unsigned int data_size[MAX_PLANE_NUM];
  unsigned char *vaddr[MAX_PLANE_NUM];
  GFX_Size size;
}GFX_Buf;



typedef void* GFX_Handle;



typedef enum
{
    GFX_Ret_OK,
    GFX_Ret_Error,
}GFX_Return;


typedef enum {
  GFX_AML_ROTATION_0,
  GFX_AML_ROTATION_90,
  GFX_AML_ROTATION_180,
  GFX_AML_ROTATION_270,
  GFX_AML_MIRROR_X,
  GFX_AML_MIRROR_Y,
} GfxAmlRotation;


#define GFX_DEFAULT_ALPHA 0x0


/*************************************************
Function:       gfx_init
Description:    init gfx 2d module, init ge2d engine
Input:          N/A
Output:         N/A
Return:         Gfx 2d handle
*************************************************/
extern GFX_Handle gfx_init();



/*************************************************
Function:       gfx_deinit
Description:    deinit gfx 2d module, deinit ge2d engine
Input:
  handle : the handle created by gfx_init
Output:         N/A
Return:
  GFX_Ret_OK        call function success
  GFX_Ret_Error     call function failed
*************************************************/
extern GFX_Return gfx_deinit(GFX_Handle handle);



/*************************************************
Function:       gfx_sync_cmd
Description:    sync all command to HW
Input:
  handle : the handle created by gfx_init
Output:         N/A
Return:
  GFX_Ret_OK        call function success
  GFX_Ret_Error     call function failed
*************************************************/
extern GFX_Return gfx_sync_cmd(GFX_Handle handle);



/*************************************************
Function:       gfx_fillrect
Description:    fill the rectangle to the buffer
Input:
  handle : the handle created by gfx_init
  pBuf : the destination buffer
  pRect : the area in the destination buffer
  color : the color fill in the area
  sync : 1, sync operation, after function returned, the job is done
         0, async operation, push in command queue,
         need call gfx_sync_cmd to wait the command queue done
Output:         N/A
Return:
  GFX_Ret_OK        call function success
  GFX_Ret_Error     call function failed
*************************************************/
extern GFX_Return gfx_fillrect(GFX_Handle handle,
                              GFX_Buf *pBuf,
                              GFX_Rect *pRect,
                              unsigned int color,
                              int sync);



/*************************************************
Function:       gfx_drawrect
Description:    draw the rectangle to the buffer
Input:
  handle : the handle created by gfx_init
  pBuf : the destination buffer
  pRect : the area in the destination buffer
  color : the color of the border
  thickness : the thickness of the border
  sync : 1, sync operation, after function returned, the job is done
         0, async operation, push in command queue,
         need call gfx_sync_cmd to wait the command queue done
Output:         N/A
Return:
  GFX_Ret_OK        call function success
  GFX_Ret_Error     call function failed
*************************************************/
extern GFX_Return gfx_drawrect(GFX_Handle handle,
                                GFX_Buf *pBuf,
                                GFX_Rect *pRect,
                                unsigned int color,
                                unsigned int thickness,
                                int sync);



/*************************************************
Function:       gfx_fillrect_software
Description:    draw the rectangle to the buffer with CPU
                it's sync command, no need to push to command queue
Input:
  handle : the handle created by gfx_init
  pBuf : the destination buffer
  pMemory : the buffer address
  pRect : the area in the destination buffer
  color : the color of the border
Output:         N/A
Return:
  GFX_Ret_OK        call function success
  GFX_Ret_Error     call function failed
*************************************************/
extern GFX_Return gfx_fillrect_software(GFX_Handle handle,
                                        GFX_Buf *pBuf,
                                        unsigned char *pMemory,
                                        GFX_Rect *pRect,
                                        unsigned int color);



/*************************************************
Function:       gfx_drawrect_software
Description:    draw the rectangle to the buffer with CPU
                it's sync command, no need to push to command queue
Input:
  handle : the handle created by gfx_init
  pBuf : the destination buffer
  pMemory : the buffer address
  pRect : the area in the destination buffer
  color : the color of the border
  thickness : the thickness of the border
Output:         N/A
Return:
  GFX_Ret_OK        call function success
  GFX_Ret_Error     call function failed
*************************************************/
extern GFX_Return gfx_drawrect_software(GFX_Handle handle,
                                        GFX_Buf *pBuf,
                                        unsigned char *pMemory,
                                        GFX_Rect *pRect,
                                        unsigned int color,
                                        unsigned int thickness);




/*************************************************
Function:       gfx_blend
Description:    blend topBuf to bottomBuf, and write to outBuf
Input:
  handle : the handle created by gfx_init
  pBottomBuf : the bottom buffer
  pBottomRect : the area in the bottom buffer
  pTopBuf : the top buffer
  pTopRect : the area in the top buffer
  pOutBuf : the output buffer
  pOutRect : the area in the output buffer
  alpha : the alpha of topBuffer, 0~255,
  sync : 1, sync operation, after function returned, the job is done
         0, async operation, push in command queue,
         need call gfx_sync_cmd to wait the command queue done
Output:         N/A
Return:
  GFX_Ret_OK        call function success
  GFX_Ret_Error     call function failed
Others : the width and height should be same
*************************************************/
extern GFX_Return gfx_blend(GFX_Handle handle,
                            GFX_Buf *pBottomBuf,
                            GFX_Rect *pBottomRect,
                            GFX_Buf *pTopBuf,
                            GFX_Rect *pTopRect,
                            GFX_Buf *pOutBuf,
                            GFX_Rect *pOutRect,
                            unsigned char alpha,
                            int sync);



/*************************************************
Function:       gfx_stretchblit
Description:    stretchblit inBuf to outBuf
Input:
  handle : the handle created by gfx_init
  pInBuf : the input buffer
  pInRect : the area in the input buffer
  pOutBuf : the output buffer
  pOutRect : the area in the output buffer
  rotation : rotage type, see GfxAmlRotation define
  sync : 1, sync operation, after function returned, the job is done
         0, async operation, push in command queue,
         need call gfx_sync_cmd to wait the command queue done
Output:         N/A
Return:
  GFX_Ret_OK        call function success
  GFX_Ret_Error     call function failed
*************************************************/
extern GFX_Return gfx_stretchblit(GFX_Handle handle,
                                  GFX_Buf *pInBuf,
                                  GFX_Rect *pInRect,
                                  GFX_Buf *pOutBuf,
                                  GFX_Rect *pOutRect,
                                  GfxAmlRotation rotation,
                                  int sync);



/*************************************************
Function:       gfx_updateDirtyArea
Description:    check dirty area, merge pNewRect to pBaseRect
                The result rect is contain the input rect
Input:
  pBuf : the input buffer
  pBaseRect : the dirty area in the input buffer
  pNewRect : the new rectangle
Output:
  pBaseRect : the output rectangle
Return:
  GFX_Ret_OK        call function success
  GFX_Ret_Error     call function failed
*************************************************/
extern GFX_Return gfx_updateDirtyArea(GFX_Buf *pBuf,
                                      GFX_Rect *pBaseRect,
                                      GFX_Rect *pNewRect);



/*************************************************
Function:       gfx_isEmptyArea
Description:    check the area, is empty or not
Input:
  pRect : the rectangle
Output:
  N/A
Return:
  1        is empty
  0       is not empty
*************************************************/
extern int gfx_isEmptyArea(GFX_Rect *pRect);


#endif /* _GFX_2D_HW_H */
