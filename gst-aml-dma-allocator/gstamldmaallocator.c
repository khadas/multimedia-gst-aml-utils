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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/allocators/gstdmabuf.h>
#include "gstamldmaallocator.h"

#include "dma_allocator.h"


GST_DEBUG_CATEGORY_STATIC(amldma_allocator_debug);
#define GST_CAT_DEFAULT amldma_allocator_debug

#define gst_amldma_allocator_parent_class parent_class

G_DEFINE_TYPE(GstAmlDMAAllocator, gst_amldma_allocator, GST_TYPE_ALLOCATOR)

static void
gst_amldma_mem_init(char *name)
{
  GstAllocator *allocator = g_object_new(gst_amldma_allocator_get_type(), NULL);
  GstAmlDMAAllocator *self = GST_AMLDMA_ALLOCATOR (allocator);

  dma_set_log_level(DMA_DEBUG_LEVEL_INFO, DMA_LOG_TERMINAL);

  gint heap_fd = aml_dmabuf_heap_open (name);
  if (heap_fd < 0) {
    GST_ERROR ("Could not open DMA buffer device:%s", name);
    g_object_unref (self);
    return;
  }

  GST_INFO("heap name:%s heap_fd=%d", name, heap_fd);
  self->heap_fd = heap_fd;

  self->dma_allocator = gst_dmabuf_allocator_new();

  gst_allocator_register(g_type_name(GST_TYPE_AMLDMA_ALLOCATOR), allocator);
}

GstAllocator*
gst_amldma_allocator_obtain(char *name)
{
  // static GOnce allocator_once = G_ONCE_INIT;
  GstAllocator *allocator;

  // g_once(&allocator_once, (GThreadFunc)gst_amldma_mem_init, (void *)name);
  gst_amldma_mem_init(name);

  allocator = gst_allocator_find(g_type_name(GST_TYPE_AMLDMA_ALLOCATOR));

  if (allocator == NULL)
    GST_WARNING("No allocator named %s found", g_type_name(GST_TYPE_AMLDMA_ALLOCATOR));

  return allocator;
}

GQuark
gst_amldma_memory_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_string ("GstAmlDMAPrivate");

  return quark;
}


static GstMemory *
gst_amldma_alloc_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  int ret = 0;
  GstAmlDMAAllocator *self = GST_AMLDMA_ALLOCATOR (allocator);
  gint dmaSize = (size + params->prefix + params->padding);

  int dmabuf_fd = -1;

  ret = aml_dmabuf_heap_alloc(self->heap_fd, dmaSize, &dmabuf_fd);

  GST_INFO("ret=%d heap_fd=%d, phyalloc dmaSize:%d dmabuf_fd: %d", ret, self->heap_fd, dmaSize, dmabuf_fd);

  GstAmlDMAMemory *dma_mem = g_slice_new0 (GstAmlDMAMemory);
  gst_memory_init (GST_MEMORY_CAST(dma_mem), GST_MEMORY_FLAG_NO_SHARE, allocator,
                   0, size, 0, 0, size);

  dma_mem->size = dmaSize;
  dma_mem->dmabuf_fd = (gint)dmabuf_fd;

  GstMemory *mem =
      gst_dmabuf_allocator_alloc (self->dma_allocator, dmabuf_fd, size);

  // GstMemory *mem = &dma_mem->mem;

  gst_mini_object_set_qdata (GST_MINI_OBJECT(mem), GST_AMLDMA_MEMORY_QUARK,
                             dma_mem, (GDestroyNotify) gst_memory_unref);
  // GstMemory *mem =
  //     gst_dmabuf_allocator_alloc (self->dma_allocator, data_fd, dmaSize);

  // gst_mini_object_set_qdata (GST_MINI_OBJECT(mem), GST_AMLDMA_MEMORY_QUARK,
  //                            dma_mem, (GDestroyNotify) gst_memory_unref);

  GST_INFO ("allocated mem %p by allocator %p with dma_mem %p\n", mem,
           allocator, dma_mem);

  // return (GstMemory *)dma_mem;
  return mem;
}

static void
gst_amldma_alloc_free (GstAllocator * allocator, GstMemory * memory)
{
  GstAmlDMAAllocator *self = GST_AMLDMA_ALLOCATOR (allocator);
  GstAmlDMAMemory *dma_mem = (GstAmlDMAMemory *) memory;

  if (!dma_mem || self->heap_fd < 0)
    return;

  GST_DEBUG ("phyfree dmaSize:%ld dmabuf_fd: %d",
             dma_mem->size, dma_mem->dmabuf_fd);

  aml_dmabuf_heap_free(dma_mem->dmabuf_fd);

  g_slice_free (GstAmlDMAMemory, dma_mem);
}

static void
gst_amldma_allocator_dispose (GObject * object)
{
  GstAmlDMAAllocator *self = GST_AMLDMA_ALLOCATOR (object);

  if (self->heap_fd) {
    close (self->heap_fd);
    self->heap_fd = -1;
  }

  if (self->dma_allocator)
    gst_object_unref(self->dma_allocator);
  self->dma_allocator = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_amldma_allocator_class_init (GstAmlDMAAllocatorClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS (klass);

  allocator_class->alloc = GST_DEBUG_FUNCPTR (gst_amldma_alloc_alloc);
  allocator_class->free = GST_DEBUG_FUNCPTR (gst_amldma_alloc_free);
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_amldma_allocator_dispose);

  GST_DEBUG_CATEGORY_INIT(amldma_allocator_debug, "amldmaallocator", 0, "DMA FD memory allocator");
}

static void
gst_amldma_allocator_init (GstAmlDMAAllocator * self)
{
  GstAllocator *allocator = GST_ALLOCATOR (self);

  allocator->mem_type = GST_ALLOCATOR_AMLDMA;
  allocator->mem_map = NULL;
  allocator->mem_unmap = NULL;
}



// gboolean
// gst_is_amldmabuf_memory (GstMemory * mem)
// {
//   g_return_val_if_fail (mem != NULL, FALSE);

//   gpointer qdata = gst_mini_object_get_qdata (GST_MINI_OBJECT(mem), GST_AMLDMA_MEMORY_QUARK);
//   return qdata != NULL;
// }


// gint gst_amldmabuf_memory_get_fd (GstMemory * mem)
// {
//   g_return_val_if_fail (mem != NULL, FALSE);
//   GstAmlDMAMemory *dma_mem = (GstAmlDMAMemory *) mem;

//   GST_INFO("gst_amldmabuf_memory_get_fd: memory:%p dmabuf_fd:%d", mem, dma_mem->dmabuf_fd);
//   return dma_mem->dmabuf_fd;
// }



// unsigned char *gst_amldmabuf_mmap (GstMemory * mem)
// {
//   g_return_val_if_fail (mem != NULL, FALSE);
//   GstAmlDMAMemory *dma_mem = (GstAmlDMAMemory *) mem;

//   GST_INFO("gst_amldmabuf_memory_get_fd: memory:%p dmabuf_fd:%d", mem, dma_mem->dmabuf_fd);
//   return mmap(NULL,
//       dma_mem->size,
//       PROT_READ | PROT_WRITE,
//       MAP_SHARED,
//       dma_mem->dmabuf_fd,
//       0);
// }



// void gst_amldmabuf_munmap (unsigned char *p, GstMemory * mem)
// {
//   g_return_val_if_fail (mem != NULL, FALSE);
//   GstAmlDMAMemory *dma_mem = (GstAmlDMAMemory *) mem;

//   GST_INFO("gst_amldmabuf_memory_get_fd: memory:%p dmabuf_fd:%d", mem, dma_mem->dmabuf_fd);
//   munmap(p, dma_mem->size);
// }

