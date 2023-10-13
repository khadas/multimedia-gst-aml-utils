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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/allocators/gstdmabuf.h>
#include <gst/allocators/gstamldmaallocator.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "gfx_2d.h"
#include "gstamlbaseoverlay.h"

#include "dma_allocator.h"

// 1088 is 32 alignment
#define OVERLAY_TEMP_BUF_MAX_SIZE (1920 * 1088 * 4)


GST_DEBUG_CATEGORY_STATIC(gst_aml_base_overlay_debug);
#define GST_CAT_DEFAULT gst_aml_base_overlay_debug

#define gst_aml_overlay_parent_class parent_class
G_DEFINE_TYPE(GstAmlOverlay, gst_aml_overlay, GST_TYPE_BASE_TRANSFORM);

// #define GST_VIDEO_FORMATS                                                      \
//   "{"                                                                          \
//   " RGBA, RGBx, RGB, "                                                          \
//   " BGRA, BGR, "                                                                \
//   " YV12, NV16, NV21, UYVY, NV12,"                                             \
//   " I420"                                                                      \
//   " } "

#define GST_VIDEO_FORMATS                                                      \
  "{"                                                                          \
  " BGR, RGB, YV12, NV12"                                                      \
  " } "

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-raw, "
                    "framerate = (fraction) [0/1, MAX], "
                    "width = (int) [ 1, MAX ], "
                    "height = (int) [ 1, MAX ], "
                    "format = (string) " GST_VIDEO_FORMATS));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-raw, "
                    "framerate = (fraction) [0/1, MAX], "
                    "width = (int) [ 1, MAX ], "
                    "height = (int) [ 1, MAX ], "
                    "format = (string) " GST_VIDEO_FORMATS));

GType gst_aml_overlay_pos_get_type(void) {
  static GType aml_overlay_pos_type = 0;
  static const GEnumValue aml_overlay_pos[] = {
      {GST_AMLOVERLAY_POS_TOP_LEFT, "top-left", "top left"},
      {GST_AMLOVERLAY_POS_TOP_MID, "top-mid", "top middle"},
      {GST_AMLOVERLAY_POS_TOP_RIGHT, "top-right", "top right"},
      {GST_AMLOVERLAY_POS_MID_LEFT, "mid-left", "middle left"},
      {GST_AMLOVERLAY_POS_MID_RIGHT, "mid-right", "middle right"},
      {GST_AMLOVERLAY_POS_CENTER, "center", "center"},
      {GST_AMLOVERLAY_POS_BOTTOM_LEFT, "bot-left", "bottom left"},
      {GST_AMLOVERLAY_POS_BOTTOM_MID, "bot-mid", "bottom middle"},
      {GST_AMLOVERLAY_POS_BOTTOM_RIGHT, "bot-right", "bottom right"},
      {0, NULL, NULL},
  };

  if (!aml_overlay_pos_type) {
    aml_overlay_pos_type =
        g_enum_register_static("GstAMLOverlayPos", aml_overlay_pos);
  }
  return aml_overlay_pos_type;
}



void gst_aml_overlay_init_surface_struct(struct aml_overlay_surface *data,
                                         GMutex *m) {
  data->handle = NULL;
  data->mutex = m;
  data->srcid = 0;
}

void gst_aml_overlay_deinit_surface_struct(struct aml_overlay_surface *data) {
  if (data->srcid) {
    g_source_remove(data->srcid);
    data->srcid = 0;
  }

}


void gst_aml_overlay_calc_text_pos(gint string_width, gint font_height,
                                   gint width, gint height,
                                   GstAmlOverlayPOS pos, gint *x, gint *y) {

  gint xpos, ypos;
  gint hvpad = font_height / 2;
  if (hvpad > 20)
    hvpad = 20;
  switch (pos) {
  case GST_AMLOVERLAY_POS_TOP_LEFT:
    xpos = hvpad;
    ypos = hvpad;
    break;

  case GST_AMLOVERLAY_POS_TOP_MID:
    xpos = (width - string_width) / 2;
    ypos = hvpad;
    break;

  case GST_AMLOVERLAY_POS_TOP_RIGHT:
    xpos = width - string_width - hvpad;
    ypos = hvpad;
    break;

  case GST_AMLOVERLAY_POS_MID_LEFT:
    xpos = hvpad;
    ypos = (height - font_height) / 2;
    break;

  case GST_AMLOVERLAY_POS_MID_RIGHT:
    xpos = width - string_width - hvpad;
    ypos = (height - font_height) / 2;
    break;

  case GST_AMLOVERLAY_POS_CENTER:
    xpos = (width - string_width) / 2;
    ypos = (height - font_height) / 2;
    break;

  case GST_AMLOVERLAY_POS_BOTTOM_LEFT:
    xpos = hvpad;
    ypos = height - font_height - hvpad;
    break;

  case GST_AMLOVERLAY_POS_BOTTOM_MID:
    xpos = (width - string_width) / 2;
    ypos = height - font_height - hvpad;
    break;

  case GST_AMLOVERLAY_POS_BOTTOM_RIGHT:
    xpos = width - string_width - hvpad;
    ypos = height - font_height - hvpad;
    break;
  default:
    xpos = hvpad;
    ypos = hvpad;
    break;
  }

  if (xpos < 0 || xpos > width)
    xpos = 0;
  if (ypos < 0 || ypos > height)
    ypos = 0;

  if (x)
    *x = xpos;
  if (y)
    *y = ypos;
}

static gboolean gst_aml_overlay_set_caps(GstBaseTransform *trans, GstCaps *in,
                                         GstCaps *out) {
  GstAmlOverlay *self = GST_AMLOVERLAY(trans);
  GstVideoInfo info;

  GST_INFO_OBJECT(self, "Enter");

  if (!gst_video_info_from_caps(&info, in)) {
    GST_ERROR_OBJECT(trans, "in caps are invalid");
    return FALSE;
  }
  self->info = info;
  self->is_info_set = TRUE;

  return TRUE;
}

static void gst_aml_overlay_init(GstAmlOverlay *self) {
  GST_INFO_OBJECT(self, "Enter");
  self->is_info_set = FALSE;
  self->graphic.size = 0;

  self->dmabuf_alloc = NULL;
  self->graphic.m_gfxhandle = NULL;
  self->graphic.m_input.memory = NULL;
  self->graphic.m_output.memory = NULL;

  for (int i=0; i<RENDER_BUF_CNT; i++) {
    self->graphic.m_render[i].memory = NULL;
  }

  g_mutex_init(&self->graphic.surface_lock);
}


static gboolean gst_aml_overlay_start(GstBaseTransform *trans) {
  GstAmlOverlay *self = GST_AMLOVERLAY(trans);

  GST_INFO_OBJECT(self, "Enter");

  // init 2d gfx engine
  self->graphic.m_gfxhandle = gfx_init();
  if (self->graphic.m_gfxhandle == NULL) {
    GST_ERROR_OBJECT(self, "failed to initialize gfx2d");
    return FALSE;
  }
  return TRUE;
}

static gboolean gst_aml_overlay_stop(GstBaseTransform *trans) {
  GstAmlOverlay *self = GST_AMLOVERLAY(trans);

  GST_INFO_OBJECT(self, "Enter");

  self->graphic.size = 0;

  if (self->graphic.m_output.memory) {
    gst_memory_unref(self->graphic.m_output.memory);
    self->graphic.m_output.memory = NULL;
  }

  for (int i=0; i<RENDER_BUF_CNT; i++) {
    if (self->graphic.m_render[i].memory) {
      gst_memory_unref(self->graphic.m_render[i].memory);
      self->graphic.m_render[i].memory = NULL;
    }
  }

  if (self->graphic.m_input.memory) {
    gst_memory_unref(self->graphic.m_input.memory);
    self->graphic.m_input.memory = NULL;
  }

  if (self->dmabuf_alloc) {
    gst_object_unref(self->dmabuf_alloc);
    self->dmabuf_alloc = NULL;
  }

  if (self->graphic.m_gfxhandle) {
    gfx_deinit(self->graphic.m_gfxhandle);
    self->graphic.m_gfxhandle = NULL;
  }

  return TRUE;
}

// 0, no check, >0, check memory overwrite (e.g. 100)
#define CHECK_MEM_OVERWRITE  0


static void gst_aml_memory_overwrite_set_magic(GstBaseTransform *trans, GstMemory *memory, int size) {
  GstAmlOverlay *self = GST_AMLOVERLAY(trans);
  if (CHECK_MEM_OVERWRITE <=0 ) return;
  GST_INFO_OBJECT(self, "Enter");

  GstMapInfo mapinfo;
  if (!gst_memory_map(memory, &mapinfo, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT(self, "failed to map memory(%p)", memory);
    return;
  }
  unsigned char *pData = mapinfo.data;
  // unsigned char *pData = gst_amldmabuf_mmap(memory);
  if (CHECK_MEM_OVERWRITE>0) {
    memset(pData+size, 0x4D, CHECK_MEM_OVERWRITE);
  }
  gst_memory_unmap(memory, &mapinfo);
  // if (pData) gst_amldmabuf_munmap(pData, memory);
  GST_INFO_OBJECT(self, "Leave");
}


static void gst_aml_memory_overwrite_check_magic(GstBaseTransform *trans, GstMemory *memory, int size) {
  GstAmlOverlay *self = GST_AMLOVERLAY(trans);
  if (CHECK_MEM_OVERWRITE <=0 ) return;
  GST_INFO_OBJECT(self, "Enter");

  GstMapInfo mapinfo;
  if (!gst_memory_map(memory, &mapinfo, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT(self, "failed to map memory(%p)", memory);
    return;
  }

  unsigned char *pData = mapinfo.data;
  // unsigned char *pData = gst_amldmabuf_mmap(memory);
  if (pData[size+CHECK_MEM_OVERWRITE-1] != 0x4D) {
    GST_ERROR_OBJECT(self, "memory overflow, data=%x", pData[size+CHECK_MEM_OVERWRITE-1]);
  }
  gst_memory_unmap(memory, &mapinfo);
  // if (pData) gst_amldmabuf_munmap(pData, memory);
  GST_INFO_OBJECT(self, "Leave");
}


static void gst_aml_overlay_before_transform(GstBaseTransform *trans,
                                             GstBuffer *outbuf) {
  GstAmlOverlay *self = GST_AMLOVERLAY(trans);
  GST_INFO_OBJECT(self, "Enter");

  if (gst_base_transform_is_passthrough(trans))
    return;

  if (self->dmabuf_alloc == NULL) {
    self->dmabuf_alloc = gst_amldma_allocator_obtain("heap-gfx");
    if (self->dmabuf_alloc == NULL)
      return;
  }

  GstMemory *input_memory = gst_buffer_get_memory(outbuf, 0);
  gboolean is_dmabuf = gst_is_dmabuf_memory(input_memory);

  // for first in, need create overlay_process
  int FirstIn=0;

  if (is_dmabuf) {
    GST_INFO_OBJECT(self, "input is dma buffer");
    self->graphic.m_input.fd = gst_dmabuf_memory_get_fd(input_memory);
    self->inputbuf_type = AMLOVERLAY_DMABUF;
    self->graphic.m_input.memory = input_memory;
  } else {
    GST_INFO_OBJECT(self, "input is not dma buffer, allocate DMA memory, info.size %ld", self->info.size);
    self->inputbuf_type = AMLOVERLAY_USRBUF;
  }
  gst_memory_unref(input_memory);


  if (!is_dmabuf) {
    GstMapInfo bufinfo;

    // create new dma memory reference
    if (self->graphic.m_input.memory == NULL) {
      self->graphic.m_input.memory =
          gst_allocator_alloc(self->dmabuf_alloc, self->info.size+CHECK_MEM_OVERWRITE, NULL);
      if (self->graphic.m_input.memory == NULL) {
        GST_ERROR_OBJECT(self, "failed to allocate input buffer, size=%ld", self->info.size);
        return;
      }
      self->graphic.m_input.fd =
          gst_dmabuf_memory_get_fd(self->graphic.m_input.memory);
      self->graphic.m_input.size = self->info.size;

      GST_INFO_OBJECT(self, "new allocated dma buffer, m_input.memory=%p, m_input.fd=%d, info.size=%ld",
        self->graphic.m_input.memory, self->graphic.m_input.fd, self->info.size);

      // set magic for check memory overwrite
      gst_aml_memory_overwrite_set_magic(trans, self->graphic.m_input.memory, self->info.size);
    }

    GstMapInfo mapinfo;
    if (!gst_memory_map(self->graphic.m_input.memory, &mapinfo, GST_MAP_READWRITE)) {
      GST_ERROR_OBJECT(self, "failed to map memory(%p)", self->graphic.m_input.memory);
      return;
    }
    unsigned char *pData = mapinfo.data;
    // unsigned char *pData = gst_amldmabuf_mmap(self->graphic.m_input.memory);

    if (!gst_buffer_map(outbuf, &bufinfo, GST_MAP_READ)) {
      GST_ERROR_OBJECT(self, "failed to map input buffer");
      gst_memory_unmap(self->graphic.m_input.memory, &mapinfo);
      // if (pData) gst_amldmabuf_munmap(pData, self->graphic.m_input.memory);
      return;
    }

    struct timeval st;
    struct timeval ed;
    double time_total;
    gettimeofday(&st, NULL);

    memcpy(pData, bufinfo.data, self->info.size);

    gettimeofday(&ed, NULL);
    time_total = (ed.tv_sec - st.tv_sec)*1000000.0 + (ed.tv_usec - st.tv_usec);

    GST_INFO_OBJECT(self, "copy to DMA buf done, pData=%p, bufinfo.data=%p, bufinfo.size=%ld, time=%lf uS",
      pData, bufinfo.data, bufinfo.size, time_total);

    gst_buffer_unmap(outbuf, &bufinfo);
    gst_memory_unmap(self->graphic.m_input.memory, &mapinfo);
    // if (pData) gst_amldmabuf_munmap(pData, self->graphic.m_input.memory);

    if (CHECK_MEM_OVERWRITE>0) {
      gst_aml_memory_overwrite_check_magic(trans, self->graphic.m_input.memory, self->graphic.m_input.size);
    }
  }

  // assume PIXEL_FORMAT_BGRA_8888
  gint pitch = (((self->info.width * 4) + 31) & ~31); // 32 alignment
  gint size = pitch * self->info.height;

  if (size > OVERLAY_TEMP_BUF_MAX_SIZE) {
    GST_ERROR_OBJECT(self, "render buffer oversize [%dx%d], size=%d, pitch=%d, alignedHeight=%d, ignored",
      self->info.width, self->info.height, size, pitch, self->info.height);
    return;
  }

  // allocate m_output DMA buffers
  if (self->graphic.m_output.memory == NULL) {
    self->graphic.m_output.memory =
        gst_allocator_alloc(self->dmabuf_alloc, size+CHECK_MEM_OVERWRITE, NULL);
    if (self->graphic.m_output.memory == NULL) {
      GST_ERROR_OBJECT(self, "failed to allocate output buffer");
      return;
    }
    self->graphic.m_output.fd =
        gst_dmabuf_memory_get_fd(self->graphic.m_output.memory);
    self->graphic.m_output.size = size;

    // fleet temp for check overwrite
    gst_aml_memory_overwrite_set_magic(trans, self->graphic.m_output.memory, size);

    GST_INFO_OBJECT(self, "m_output.memory=%p, m_output.fd=%d size=%d", self->graphic.m_output.memory, self->graphic.m_output.fd, size);

    // first time, need start processor
    FirstIn = 1;
  }

  // allocate m_render DMA buffers
  for (int i=0; i<RENDER_BUF_CNT; i++) {
    if (NULL == self->graphic.m_render[i].memory) {
      self->graphic.m_render[i].memory =
          gst_allocator_alloc(self->dmabuf_alloc, size+CHECK_MEM_OVERWRITE, NULL);
      if (self->graphic.m_render[i].memory == NULL) {
        GST_ERROR_OBJECT(self, "failed to allocate render[%d] buffer, size=%d", i, size);
        return;
      }
      self->graphic.m_render[i].fd =
          gst_dmabuf_memory_get_fd(self->graphic.m_render[i].memory);
      self->graphic.m_render[i].size = size;

      // fleet temp for check overwrite
      gst_aml_memory_overwrite_set_magic(trans, self->graphic.m_render[i].memory, size);

      GST_INFO_OBJECT(self, "[%d]m_render.memory=%p, m_render.fd=%d size=%d",
        i, self->graphic.m_render[i].memory, self->graphic.m_render[i].fd, size);
    }
  }

  if (1 == FirstIn && self->process)
  {
    self->m_thread = g_thread_new("overlay process", self->process, self);
  }

  if (CHECK_MEM_OVERWRITE>0) {
    // fleet temp for checking memory overwrite
    gst_aml_memory_overwrite_check_magic(trans, self->graphic.m_output.memory, self->graphic.m_output.size);
    for (int i=0; i<RENDER_BUF_CNT; i++) {
      gst_aml_memory_overwrite_check_magic(trans, self->graphic.m_render[i].memory, self->graphic.m_render[i].size);
    }
  }
}



// inherit class will call this as after transform ip action
static GstFlowReturn gst_aml_overlay_transform_ip(GstBaseTransform *trans,
                                                  GstBuffer *outbuf) {
  GstAmlOverlay *self = GST_AMLOVERLAY(trans);

  GST_INFO_OBJECT(self, "Enter");


  if (self->inputbuf_type == AMLOVERLAY_USRBUF &&
      self->graphic.m_input.memory != NULL) {
    struct timeval st;
    struct timeval ed;
    double time_total;
    gettimeofday(&st, NULL);

    // not dma buffer
    // buffer write back
    GstMapInfo mapinfo;
    if (!gst_memory_map(self->graphic.m_input.memory, &mapinfo, GST_MAP_READWRITE)) {
      GST_ERROR_OBJECT(self, "failed to map memory(%p)", self->graphic.m_input.memory);
      return GST_FLOW_ERROR;
    }
    unsigned char *pData = mapinfo.data;
    // unsigned char *pData = gst_amldmabuf_mmap(self->graphic.m_input.memory);

    GstMapInfo bufinfo;
    if (!gst_buffer_map(outbuf, &bufinfo, GST_MAP_WRITE)) {
      GST_ERROR_OBJECT(self, "failed to map input buffer");
      gst_memory_unmap(self->graphic.m_input.memory, &mapinfo);
      // if (pData) gst_amldmabuf_munmap(pData, self->graphic.m_input.memory);
      return GST_FLOW_ERROR;
    }

    memcpy(bufinfo.data, pData, bufinfo.size);
    gst_buffer_unmap(outbuf, &bufinfo);

    gst_memory_unmap(self->graphic.m_input.memory, &mapinfo);
    // if (pData) gst_amldmabuf_munmap(pData, self->graphic.m_input.memory);

    gettimeofday(&ed, NULL);
    time_total = (ed.tv_sec - st.tv_sec)*1000000.0 + (ed.tv_usec - st.tv_usec);

    GST_INFO_OBJECT(self, "surface writeback end time=%lf uS", time_total);
  }

  return GST_FLOW_OK;
}

/* initialize the aml_overlay's class */
static void gst_aml_overlay_class_init(GstAmlOverlayClass *klass) {
  GstElementClass *gstelement_class;

  gstelement_class = (GstElementClass *)klass;

  GST_DEBUG_CATEGORY_INIT(gst_aml_base_overlay_debug, "aml_base_overlay", 0,
                          "amlogic base overlay");

  gst_element_class_add_pad_template(
      gstelement_class, gst_static_pad_template_get(&src_template));
  gst_element_class_add_pad_template(
      gstelement_class, gst_static_pad_template_get(&sink_template));

  GST_BASE_TRANSFORM_CLASS(klass)->set_caps =
      GST_DEBUG_FUNCPTR(gst_aml_overlay_set_caps);

  GST_BASE_TRANSFORM_CLASS(klass)->start =
      GST_DEBUG_FUNCPTR(gst_aml_overlay_start);

  GST_BASE_TRANSFORM_CLASS(klass)->stop =
      GST_DEBUG_FUNCPTR(gst_aml_overlay_stop);

  GST_BASE_TRANSFORM_CLASS(klass)->before_transform =
      GST_DEBUG_FUNCPTR(gst_aml_overlay_before_transform);

  GST_BASE_TRANSFORM_CLASS(klass)->transform_ip =
      GST_DEBUG_FUNCPTR(gst_aml_overlay_transform_ip);

  GST_BASE_TRANSFORM_CLASS(klass)->transform_ip_on_passthrough = FALSE;
}

