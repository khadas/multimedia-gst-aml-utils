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
#ifndef _DMABUF_ALLOCATOR_H
#define _DMABUF_ALLOCATOR_H

#include "dma_log.h"



/*************************************************
Function:       aml_dmabuf_heap_open
Description:    open DMA heap pool
Input:
  name : the heap pool name
Output:         N/A
Return:
  >=0 : success, return the file handle of DMA heap
  <0  : failed
*************************************************/
int aml_dmabuf_heap_open(char *name);


/*************************************************
Function:       aml_dmabuf_heap_alloc
Description:    allocate memory from DMA heap pool
Input:
  dev_fd : the file handle of DMA heap
  len : the size of request
Output:
  dmabuf_fd : the file handle of DMA buffer
Return:
  >=0 : success
  <0  : failed
*************************************************/
int aml_dmabuf_heap_alloc(int dev_fd, size_t len, int *dmabuf_fd);

/*************************************************
Function:       aml_dmabuf_heap_free
Description: free memory from DMA heap pool
Input:
  dmabuf_fd : the file handle of DMA buffer
Output:         N/A
Return:
  >=0 : success
  <0  : failed
*************************************************/
int aml_dmabuf_heap_free(int dmabuf_fd);


#endif /* _DMABUF_ALLOCATOR_H */
