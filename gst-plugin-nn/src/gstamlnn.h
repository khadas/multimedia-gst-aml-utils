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

#ifndef __GST_AMLNN_H__
#define __GST_AMLNN_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <nn_detect.h>

G_BEGIN_DECLS

#define GST_TYPE_AMLNN \
  (gst_aml_nn_get_type())
#define GST_AMLNN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AMLNN,GstAmlNN))
#define GST_AMLNN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AMLNN,GstAmlNNClass))
#define GST_IS_AMLNN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AMLNN))
#define GST_IS_AMLNN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AMLNN))

typedef struct _GstAmlNN      GstAmlNN;
typedef struct _GstAmlNNClass GstAmlNNClass;


#define NN_BUF_CNT 3

typedef struct _model_info {
  det_model_type model;
  det_param_t param;
  gboolean initialized;
  gint width;
  gint height;
  gint channel;
  gint rowbytes;
  gint stride;

  struct {
    GstMemory *memory;
    gint fd;
    gint size;
  } nn_input[NN_BUF_CNT];
  // GstMemory *outmem;

  // for nn_inout buffer
  GMutex buffer_lock;

  // current render buffer index
  int next_nn_idx;
  int cur_nn_idx;
  int prepare_idx;
} ModelInfo;


typedef struct _thread_info {
  /// thread handle
  GThread *m_thread;

  // mutex for condition
  GMutex m_mutex;
  GCond m_cond;
  gboolean m_ready;
  gboolean m_running;

} ThreadInfo;


struct _GstAmlNN {
  GstBaseTransform element;

  /*< private >*/
  ModelInfo face_det;
  gint max_detect_num;

  GstAllocator *dmabuf_alloc;

  // gfx2d handle
  void *m_gfxhandle;

  // nn thread
  ThreadInfo m_nn_thread;

  // postprocess thread
  ThreadInfo m_pp_thread;

  GstVideoInfo info;
  gboolean is_info_set;
};

struct _GstAmlNNClass {
  GstBaseTransformClass parent_class;
};

GType gst_aml_nn_get_type (void);

G_END_DECLS

#endif /* __GST_AMLNN_H__ */
