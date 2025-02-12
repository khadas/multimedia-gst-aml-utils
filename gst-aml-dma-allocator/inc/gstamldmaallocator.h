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

#ifndef __GST_AML_DMA_ALLOCATOR_H__
#define __GST_AML_DMA_ALLOCATOR_H__

#include <gst/gst.h>
#include <gst/gstallocator.h>

G_BEGIN_DECLS

typedef struct _GstAmlDMAAllocator GstAmlDMAAllocator;
typedef struct _GstAmlDMAAllocatorClass GstAmlDMAAllocatorClass;
typedef struct _GstAmlDMAMemory GstAmlDMAMemory;

#define GST_ALLOCATOR_AMLDMA "amldma"

#define GST_TYPE_AMLDMA_ALLOCATOR gst_amldma_allocator_get_type ()
#define GST_IS_AMLDMA_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
    GST_TYPE_AMLDMA_ALLOCATOR))
#define GST_AMLDMA_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AMLDMA_ALLOCATOR, GstAmlDMAAllocator))
#define GST_AMLDMA_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AMLDMA_ALLOCATOR, GstAmlDMAAllocatorClass))
#define GST_AMLDMA_ALLOCATOR_CAST(obj) ((GstAmlDMAAllocator *)(obj))

#define GST_AMLDMA_MEMORY_QUARK gst_amldma_memory_quark ()

struct _GstAmlDMAAllocator
{
  GstAllocator parent;

  gint heap_fd;  // device fd
  GstAllocator *dma_allocator;
};

struct _GstAmlDMAAllocatorClass
{
  GstAllocatorClass parent;
};

struct _GstAmlDMAMemory {
  GstMemory mem;

  gint dmabuf_fd;  // DMA buffer fd
  gsize size;
};

GType gst_amldma_allocator_get_type (void);
GstAllocator* gst_amldma_allocator_obtain (char *name);


// The following API no useful now, use standard API
// gboolean gst_is_amldmabuf_getfd (GstMemory * mem);
// gint gst_amldmabuf_memory_get_fd (GstMemory * mem);
// unsigned char *gst_amldmabuf_mmap (GstMemory * mem);
// void gst_amldmabuf_munmap (unsigned char *p, GstMemory * mem);


G_END_DECLS

#endif /* __GST_AML_DMA_ALLOCATOR_H__ */
