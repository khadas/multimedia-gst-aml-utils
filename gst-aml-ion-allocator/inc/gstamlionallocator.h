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

#ifndef __GST_AMLAmlIONALLOCATOR_H__
#define __GST_AMLAmlIONALLOCATOR_H__

#include <gst/gst.h>
#include <gst/gstallocator.h>

G_BEGIN_DECLS

typedef struct _GstAmlIONAllocator GstAmlIONAllocator;
typedef struct _GstAmlIONAllocatorClass GstAmlIONAllocatorClass;
typedef struct _GstAmlIONMemory GstAmlIONMemory;

#define GST_ALLOCATOR_AMLION "amlionmem"

#define GST_TYPE_AMLION_ALLOCATOR gst_amlion_allocator_get_type ()
#define GST_IS_AMLION_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
    GST_TYPE_AMLION_ALLOCATOR))
#define GST_AMLION_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AMLION_ALLOCATOR, GstAmlIONAllocator))
#define GST_AMLION_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AMLION_ALLOCATOR, GstAmlIONAllocatorClass))
#define GST_AMLION_ALLOCATOR_CAST(obj) ((GstAmlIONAllocator *)(obj))

#define GST_AMLION_MEMORY_QUARK gst_amlion_memory_quark ()

struct _GstAmlIONAllocator
{
  GstAllocator parent;

  gint fd;
  GstAllocator *dma_allocator;
};

struct _GstAmlIONAllocatorClass
{
  GstAllocatorClass parent;
};

struct _GstAmlIONMemory {
  GstMemory mem;

  gint fd;
  gsize size;
};

GType gst_amlion_allocator_get_type (void);
GstAllocator* gst_amlion_allocator_obtain (void);
gboolean gst_is_amlionbuf_memory (GstMemory * mem);

G_END_DECLS

#endif /* __GST_AMLAmlIONALLOCATOR_H__ */
