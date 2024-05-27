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

/**
 * SECTION:element-amlnn
 *
 * FIXME:Describe amlnn here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * mipi camera in yocto:
 * gst-launch-1.0 v4l2src device=/dev/media0 ! video/x-raw,width=1920,height=1080 ! amlnn ! amlnnoverlay ! glimagesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gmodule.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstamlnn.h"
#include <gst/allocators/gstdmabuf.h>
#include <gst/allocators/gstamldmaallocator.h>

#include "gfx_2d.h"
#include "gst_ge2d.h"

// for test time
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


#define PRINT_FPS

#ifdef PRINT_FPS
#define PRINT_FPS_INTERVAL_S 120

static int64_t get_current_time_msec(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif


GST_DEBUG_CATEGORY_STATIC(gst_aml_nn_debug);
#define GST_CAT_DEFAULT gst_aml_nn_debug


// must sync with gstamlnnoverlay.h
//////////////////////////////////////////////////////////////////////
typedef struct _NNRect {
  float left;
  float top;
  float right;
  float bottom;
}NNRect;

typedef struct _NNPoint {
  float x;
  float y;
}NNPoint;

#define MAX_NN_LABEL_LENGTH 256
typedef struct _NNResult {
  NNRect pos;
  NNPoint fpos[5];
  int label_id;                        ///> Classification of label ID
  char label_name[MAX_NN_LABEL_LENGTH];   ///> Label name
}NNResult;

typedef struct _NNResultBuffer {
  gint amount; // amount of result
  NNResult *results;
}NNResultBuffer;
//////////////////////////////////////////////////////////////////////


#define DEFAULT_PROP_FACE_DET_MODEL DET_AML_FACE_DETECTION

#define DEFAULT_PROP_MAX_DET_NUM DETECT_NUM


#define NN_INPUT_BUF_FORMAT GST_VIDEO_FORMAT_RGB


enum {
  PROP_0,
  PROP_FACE_DET_MODEL,
  PROP_MAX_DET_NUM,
};

#define GST_TYPE_AML_FACE_DET_MODEL (gst_aml_face_det_model_get_type())
static GType gst_aml_face_det_model_get_type(void) {
  static GType aml_face_det_model = 0;
  static const GEnumValue aml_detection_models[] = {
      {DET_AML_FACE_DETECTION, "amlfd", "aml_face_detection"},
      {DET_YOLO_V3, "yolov3", "yolo v3"},
      {0, NULL, NULL},
  };

  if (!aml_face_det_model) {
    aml_face_det_model =
        g_enum_register_static("GstAMLFaceDetectModel", aml_detection_models);
  }
  return aml_face_det_model;
}


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

#define gst_aml_nn_parent_class parent_class
G_DEFINE_TYPE(GstAmlNN, gst_aml_nn, GST_TYPE_BASE_TRANSFORM);

static void gst_aml_nn_set_property(GObject *object, guint prop_id,
                                    const GValue *value, GParamSpec *pspec);
static void gst_aml_nn_get_property(GObject *object, guint prop_id,
                                    GValue *value, GParamSpec *pspec);

static gboolean gst_aml_nn_open(GstBaseTransform *base);

static gboolean gst_aml_nn_close(GstBaseTransform *base);

static void gst_aml_nn_finalize(GObject *object);

static GstFlowReturn gst_aml_nn_transform_ip(GstBaseTransform *base,
                                             GstBuffer *outbuf);

static gboolean gst_aml_nn_set_caps(GstBaseTransform *base, GstCaps *incaps,
                                    GstCaps *outcaps);

static gpointer amlnn_process(void *data);
static gpointer amlnn_post_process(void *data);
static void push_result(GstBaseTransform *base, NNResultBuffer *resbuf);
/* GObject vmethod implementations */
static gboolean gst_aml_nn_event(GstBaseTransform *trans,
                                         GstEvent *event);

/* initialize the amlnn's class */
static void gst_aml_nn_class_init(GstAmlNNClass *klass) {
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;

  gobject_class->set_property = gst_aml_nn_set_property;
  gobject_class->get_property = gst_aml_nn_get_property;
  gobject_class->finalize = gst_aml_nn_finalize;

  g_object_class_install_property(
      G_OBJECT_CLASS(klass), PROP_FACE_DET_MODEL,
      g_param_spec_enum("detection-model", "detection-model",
                        "face detection model", GST_TYPE_AML_FACE_DET_MODEL,
                        DEFAULT_PROP_FACE_DET_MODEL,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(
      gobject_class, PROP_MAX_DET_NUM,
      g_param_spec_int(
          "max-detect-num", "max-detection-number", "maximum detection number", 10, 230,
          DEFAULT_PROP_MAX_DET_NUM, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_details_simple(gstelement_class, "amlnn",
                                       "Generic/Filter", "Amlogic NN module",
                                       "Guoping Li <guoping.li@amlogic.com>");

  gst_element_class_add_pad_template(
      gstelement_class, gst_static_pad_template_get(&src_template));

  gst_element_class_add_pad_template(
      gstelement_class, gst_static_pad_template_get(&sink_template));

  GST_BASE_TRANSFORM_CLASS(klass)->transform_ip =
      GST_DEBUG_FUNCPTR(gst_aml_nn_transform_ip);

  GST_BASE_TRANSFORM_CLASS(klass)->set_caps =
      GST_DEBUG_FUNCPTR(gst_aml_nn_set_caps);

  GST_BASE_TRANSFORM_CLASS(klass)->start = GST_DEBUG_FUNCPTR(gst_aml_nn_open);
  GST_BASE_TRANSFORM_CLASS(klass)->stop = GST_DEBUG_FUNCPTR(gst_aml_nn_close);

  GST_BASE_TRANSFORM_CLASS(klass)->transform_ip_on_passthrough = FALSE;

  GST_BASE_TRANSFORM_CLASS(klass)->src_event =
      GST_DEBUG_FUNCPTR(gst_aml_nn_event);
}


/* initialize the new element
 * initialize instance structure
 */
static void gst_aml_nn_init(GstAmlNN *self) {
  self->is_info_set = FALSE;

  memset(&self->face_det, 0, sizeof(ModelInfo));
  self->face_det.model = DEFAULT_PROP_FACE_DET_MODEL;
  self->max_detect_num = DEFAULT_PROP_MAX_DET_NUM;

  self->m_gfxhandle = NULL;
  self->dmabuf_alloc = NULL;

  g_mutex_init(&self->face_det.buffer_lock);
  self->face_det.prepare_idx = -1;
  self->face_det.cur_nn_idx = -1;
  self->face_det.next_nn_idx = -1;

  self->pr_ready = TRUE;
  g_mutex_init(&self->pr_mutex);

  // nn main thread
  ThreadInfo *pThread = &self->m_nn_thread;
  g_cond_init(&pThread->m_cond);
  g_mutex_init(&pThread->m_mutex);
  pThread->m_ready = FALSE;

  // nn post process thread
  pThread = &self->m_pp_thread;
  g_cond_init(&pThread->m_cond);
  g_mutex_init(&pThread->m_mutex);
  pThread->m_ready = FALSE;

}

static gboolean open_model(ModelInfo *m) {
  if (m->initialized)
    return TRUE;

  if (m->model == DET_BUTT)
    return FALSE;

  if (det_set_model(m->model) != DET_STATUS_OK) {
    return FALSE;
  }

  if (det_get_param(m->model, &m->param) != DET_STATUS_OK) {
    return FALSE;
  }

  if (det_get_model_size(m->model, &m->width, &m->height, &m->channel) !=
      DET_STATUS_OK) {
    det_release_model(m->model);
    return FALSE;
  }

  m->rowbytes = m->width * m->channel;
  m->stride = (m->rowbytes + 31) & ~31;
  m->initialized = TRUE;
  return TRUE;
}

static gboolean close_model(ModelInfo *m) {
  if (!m->initialized)
    return TRUE;

  for (int i=0; i<NN_BUF_CNT; i++) {
    if (m->nn_input[i].memory) {
      gst_memory_unref (m->nn_input[i].memory);
      m->nn_input[i].memory = NULL;
    }
  }

  if (m->model == DET_BUTT)
    return FALSE;

  if (det_release_model(m->model) != DET_STATUS_OK) {
    return FALSE;
  }

  m->initialized = FALSE;
  return TRUE;
}


struct idle_task_data {
  GstAmlNN *self;
  union {
    struct _model {
      ModelInfo *minfo;
      det_model_type new_model;
    } model;
  } u;
};

static gboolean idle_close_model(struct idle_task_data *data) {
  if (data == NULL || data->self == NULL ||
      data->u.model.minfo == NULL) {
    return G_SOURCE_REMOVE;
  }

  GstAmlNN *self = data->self;
  ModelInfo *minfo = data->u.model.minfo;

  ThreadInfo *pThread = &self->m_nn_thread;
  ThreadInfo *pPPThread = &self->m_pp_thread;
  g_mutex_lock(&pThread->m_mutex);

  if (pThread->m_running == FALSE &&
      pPPThread->m_running == FALSE) {
    return G_SOURCE_REMOVE;
  }

  // close detect model for the next reinitialization
  close_model(minfo);
  minfo->model = data->u.model.new_model;

  g_mutex_lock(&pPPThread->m_mutex);
  if (data->u.model.new_model == DET_BUTT) {
    // notify the overlay to clear info
    push_result (&self->element, NULL);
  }
  g_mutex_unlock(&pPPThread->m_mutex);
  g_mutex_unlock(&pThread->m_mutex);

  g_free(data);
  return G_SOURCE_REMOVE;
}


static void gst_aml_nn_set_property(GObject *object, guint prop_id,
                                    const GValue *value, GParamSpec *pspec) {
  GstAmlNN *self = GST_AMLNN(object);

  switch (prop_id) {
  case PROP_FACE_DET_MODEL: {
    det_model_type m = g_value_get_enum(value);
    if (m != self->face_det.model) {
      // close detect model for the next reinitialization
      if (self->face_det.initialized) {
        struct idle_task_data *data = g_new(struct idle_task_data, 1);
        data->self = self;
        data->u.model.minfo = &self->face_det;
        data->u.model.new_model = m;
        g_idle_add((GSourceFunc)idle_close_model, data);
      } else {
        self->face_det.model = m;
      }
    }
  } break;
  case PROP_MAX_DET_NUM: {
    int n = g_value_get_int(value);
    if (self->face_det.initialized && n != self->max_detect_num) {
      self->max_detect_num = self->face_det.param.param.det_param.detect_num = n;
      det_set_param(self->face_det.model, self->face_det.param);
    }
  } break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void gst_aml_nn_get_property(GObject *object, guint prop_id,
                                    GValue *value, GParamSpec *pspec) {
  GstAmlNN *self = GST_AMLNN(object);

  switch (prop_id) {
  case PROP_FACE_DET_MODEL:
    g_value_set_enum(value, self->face_det.model);
    break;
  case PROP_MAX_DET_NUM:
    g_value_set_int(value, self->max_detect_num);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_aml_nn_open(GstBaseTransform *base) {
  GstAmlNN *self = GST_AMLNN(base);

  self->m_gfxhandle = gfx_init();
  if (self->m_gfxhandle == NULL) {
    GST_ERROR_OBJECT(self, "failed to initialize gfx2d");
    return FALSE;
  }

  ThreadInfo *pThread = &self->m_nn_thread;
  pThread->m_running = TRUE;
  pThread->m_thread = g_thread_new("nn-process", amlnn_process, self);

  pThread = &self->m_pp_thread;
  pThread->m_running = TRUE;
  pThread->m_thread = g_thread_new("nn-post-process", amlnn_post_process, self);
  return TRUE;
}

static gboolean gst_aml_nn_close(GstBaseTransform *base) {
  GstAmlNN *self = GST_AMLNN(base);

  GST_INFO(self, "closing, waiting for lock");

  // nn process
  ThreadInfo *pThread = &self->m_nn_thread;
  ThreadInfo *ppThread = &self->m_pp_thread;

  pThread->m_running = FALSE;
  ppThread->m_running = FALSE;
  g_mutex_lock(&pThread->m_mutex);
  pThread->m_ready = TRUE;
  g_cond_signal(&pThread->m_cond);
  g_mutex_unlock(&pThread->m_mutex);

  g_thread_join(pThread->m_thread);
  pThread->m_thread = NULL;

  GST_INFO("nn process join done ");

  // nn post process
  g_mutex_lock(&ppThread->m_mutex);
  ppThread->m_ready = TRUE;
  g_cond_signal(&ppThread->m_cond);
  g_mutex_unlock(&ppThread->m_mutex);

  g_thread_join(ppThread->m_thread);
  ppThread->m_thread = NULL;

  GST_INFO("nn post process join done");
  // gfx 2d
  if (self->m_gfxhandle) {
    gfx_deinit(self->m_gfxhandle);
    self->m_gfxhandle = NULL;
  }

  // exiting
  close_model(&self->face_det);

  GST_INFO("closed");

  return TRUE;
}



static void gst_aml_nn_finalize(GObject *object) {
  GstAmlNN *self = GST_AMLNN(object);
  GST_INFO("Enter");
  G_OBJECT_CLASS(parent_class)->finalize(object);

  g_mutex_clear(&self->pr_mutex);

  // nn main thread
  ThreadInfo *pThread = &self->m_nn_thread;
  g_cond_clear(&pThread->m_cond);
  g_mutex_clear(&pThread->m_mutex);

  // nn post process thread
  pThread = &self->m_pp_thread;
  g_cond_clear(&pThread->m_cond);
  g_mutex_clear(&pThread->m_mutex);
  self->pr_ready = FALSE;
}



static gboolean gst_aml_nn_set_caps(GstBaseTransform *base, GstCaps *incaps,
                                    GstCaps *outcaps) {
  GstAmlNN *self = GST_AMLNN(base);
  GstVideoInfo info;

  if (!gst_video_info_from_caps(&info, incaps)) {
    GST_ERROR_OBJECT(self, "caps are invalid");
    return FALSE;
  }
  self->info = info;
  self->is_info_set = TRUE;

  return TRUE;
}



static gboolean detection_init(GstAmlNN *self) {
  // open face detect model
  if (!self->face_det.initialized) {
    GST_INFO("open_model, model=%d", self->face_det.model);
    open_model(&self->face_det);
    self->face_det.param.param.det_param.detect_num = self->max_detect_num;
    det_set_param(self->face_det.model, self->face_det.param);
  }

  if (!self->face_det.initialized) {
    GST_ERROR_OBJECT(self, "detection model not initialized");
    return FALSE;
  }

  // allocate all nn input memory
  for (int i=0; i<NN_BUF_CNT; i++) {
    if (self->face_det.nn_input[i].memory == NULL) {

      gint size = self->face_det.stride * self->face_det.height;
      self->face_det.nn_input[i].memory = gst_allocator_alloc(
          self->dmabuf_alloc,
          size, NULL);
      if (NULL == self->face_det.nn_input[i].memory) {
        GST_ERROR_OBJECT(self, "failed to allocate the nn_input dma buffer, %p, %d",
          self->dmabuf_alloc, self->face_det.stride * self->face_det.height);
        return FALSE;
      }

      self->face_det.nn_input[i].fd = gst_dmabuf_memory_get_fd(self->face_det.nn_input[i].memory);
      self->face_det.nn_input[i].size = size;

      GST_INFO_OBJECT(self, "[%d]dmabuf_alloc=%p, stride=%d, height=%d, memory=%p, fd=%d", i,
          self->dmabuf_alloc, self->face_det.stride, self->face_det.height, self->face_det.nn_input[i].memory, self->face_det.nn_input[i].fd);
    }
  }

  return TRUE;
}



static gboolean detection_process(GstAmlNN *self) {
  gboolean ret = TRUE;

  GST_INFO_OBJECT(self, "Enter");

  if (self->face_det.model == DET_BUTT) {
    // face detection not enabled,
    // ignore the request and exit
    GST_DEBUG_OBJECT(self, "face detection model disabled");
    // make sure the input memory would be released
    return FALSE;
  }

  struct timeval st;
  struct timeval ed;
  double time_total;
  gettimeofday(&st, NULL);

    // get display buffer
  g_mutex_lock(&self->face_det.buffer_lock);
  int nn_idx = self->face_det.next_nn_idx;
  self->face_det.cur_nn_idx = nn_idx;
  g_mutex_unlock(&self->face_det.buffer_lock);

  GST_INFO_OBJECT(self, "start detect, nn_idx=%d, memory=%p", nn_idx, self->face_det.nn_input[nn_idx].memory);

  GstMapInfo mapinfo;
  if (!gst_memory_map(self->face_det.nn_input[nn_idx].memory, &mapinfo, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT(self, "failed to map memory(%p)", self->face_det.nn_input[nn_idx].memory);
    return FALSE;
  }
  unsigned char *pData = mapinfo.data;
  // unsigned char *pData = gst_amldmabuf_mmap(self->face_det.nn_input[nn_idx].memory);

  GST_INFO_OBJECT(self, "nn_idx=%d, rowbytes=%d, stride=%d",
    nn_idx, self->face_det.rowbytes, self->face_det.stride);

  if (self->face_det.rowbytes != self->face_det.stride) {
    gint rowbytes = self->face_det.rowbytes;
    gint stride = self->face_det.stride;
    for (int i = 0; i < self->face_det.height; i++) {
      memcpy(&pData[rowbytes * i], &pData[stride * i], rowbytes);
    }
  }

  GST_INFO_OBJECT(self, "pData=%p, width=%d, height=%d, channel=%d",
    pData, self->face_det.width, self->face_det.height, self->face_det.channel);

  input_image_t im;
  im.data = pData;
  im.pixel_format = PIX_FMT_RGB888;
  im.width = self->face_det.width;
  im.height = self->face_det.height;
  im.channel = self->face_det.channel;
  GST_INFO_OBJECT(self, "det_trigger_inference for detection");
  det_status_t rc = det_trigger_inference(im, self->face_det.model);
  gst_memory_unmap(self->face_det.nn_input[nn_idx].memory, &mapinfo);
  // if (pData) gst_amldmabuf_munmap(pData, self->face_det.nn_input[nn_idx].memory);

  GST_INFO_OBJECT(self, "detect done, nn_idx=%d", nn_idx);

  if (rc != DET_STATUS_OK) {
    GST_ERROR_OBJECT(self, "failed to det_trigger_inference");
    return FALSE;
  }

  // NPU use nn_input buffer done, update new buffer index
  g_mutex_lock(&self->face_det.buffer_lock);
  if (nn_idx == self->face_det.next_nn_idx) {
    // new nn buffer not ready, neednot update prepare buffer index
  } else {
    self->face_det.prepare_idx ++;
    if (NN_BUF_CNT == self->face_det.prepare_idx) {
      self->face_det.prepare_idx = 0;
    }
  }
  GST_INFO_OBJECT(self, "detection_process done,prepare_idx=%d next_nn_idx=%d,cur_nn_idx=%d",self->face_det.prepare_idx,self->face_det.next_nn_idx,self->face_det.cur_nn_idx);


  self->face_det.cur_nn_idx = -1;
  g_mutex_unlock(&self->face_det.buffer_lock);

  gettimeofday(&ed, NULL);
  time_total = (ed.tv_sec - st.tv_sec)*1000000.0 + (ed.tv_usec - st.tv_usec);
  GST_INFO_OBJECT(self, "Leave, det_trigger_inference done, time=%lf uS", time_total);

  return ret;
}



static gpointer amlnn_process(void *data) {
  GstAmlNN *self = (GstAmlNN *)data;

  ThreadInfo *pThread = &self->m_nn_thread;
  GST_INFO_OBJECT(self, "Enter, m_running=%d, m_ready=%d", pThread->m_running, pThread->m_ready);

  ThreadInfo *pPPThread = &self->m_pp_thread;
  while (pThread->m_running) {
    g_mutex_lock(&pThread->m_mutex);
    while (!pThread->m_ready) {
      g_cond_wait(&pThread->m_cond, &pThread->m_mutex);
    }

    GST_INFO_OBJECT(self, "wait m_cond done, model=%d", self->face_det.model);

    if (!pThread->m_running) {
      g_mutex_unlock(&pThread->m_mutex);
      continue;
    }

    pThread->m_ready = FALSE;
    g_mutex_unlock(&pThread->m_mutex);

    detection_process(self);

    // wake up pp thread to work
    if (g_mutex_trylock(&pPPThread->m_mutex)) {
      pPPThread->m_ready = TRUE;
      g_cond_signal(&pPPThread->m_cond);
      GST_INFO_OBJECT(self, "send m_cond to post process, model=%d", self->face_det.model);
      g_mutex_unlock(&pPPThread->m_mutex);
    }
  }

  GST_INFO_OBJECT(self, " exit");

  return NULL;
}



static void push_result(GstBaseTransform *base,
                        NNResultBuffer *resbuf) {
  GstMapInfo info;
  GstAmlNN *self = GST_AMLNN(base);
  ThreadInfo *PPThread = &self->m_pp_thread;
  g_mutex_lock(&self->pr_mutex);
  if (self->pr_ready) {
    if (resbuf == NULL || resbuf->amount <= 0) {
      GST_INFO_OBJECT(self, "nn-result-clear");
      GstStructure *st = gst_structure_new("nn-result-clear", NULL, NULL);
      GstEvent *event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, st);
      gst_element_send_event(&base->element, event);
      GST_INFO_OBJECT(self, "push_result nn-result-clear end");
      g_mutex_unlock(&self->pr_mutex);
      return;
    }

    GST_INFO_OBJECT(self, "resbuf->amount=%d", resbuf->amount);

    gint st_size = sizeof(NNResultBuffer);
    gint res_size = resbuf->amount * sizeof(NNResult);

    GstBuffer *gstbuf = gst_buffer_new_allocate(NULL, st_size + res_size, NULL);
    if (gst_buffer_map(gstbuf, &info, GST_MAP_WRITE)) {
      memcpy(info.data, resbuf, st_size);
      memcpy(info.data + st_size, resbuf->results, res_size);
      gst_buffer_unmap(gstbuf, &info);

      GstStructure *st = gst_structure_new("nn-result", "result-buffer",
                                           GST_TYPE_BUFFER, gstbuf, NULL);

      GstEvent *event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, st);

      GST_INFO_OBJECT(self, "gst_element_send_event to overlay");

      gst_element_send_event(&base->element, event);
      GST_INFO_OBJECT(self, "push_result result-buffer end");
    }
    gst_buffer_unref(gstbuf);
  }
  g_mutex_unlock(&self->pr_mutex);
  return;
}

static gboolean gst_aml_nn_event(GstBaseTransform *trans,
                                         GstEvent *event){
    GstAmlNN *self = GST_AMLNN(trans);
    GST_INFO_OBJECT(self, "enter");
    switch (GST_EVENT_TYPE(event)) {
        case GST_EVENT_CUSTOM_UPSTREAM: {
            GST_INFO_OBJECT(self, "gstamlnnoverlay-change-state start");
             const GstStructure *st = gst_event_get_structure(event);
            if (gst_structure_has_name(st, "gstamlnnoverlay-change-state")) {
                GST_INFO_OBJECT(self, "gstamlnnoverlay-change-state receive");
                g_mutex_lock(&self->pr_mutex);
                self->pr_ready = FALSE;
                g_mutex_unlock(&self->pr_mutex);
                GST_INFO_OBJECT(self, "nn event reslove");
            }
        }break;

        default:
            break;
    }

    return GST_BASE_TRANSFORM_CLASS(parent_class)->src_event(trans, event);
}


static gpointer amlnn_post_process(void *data) {
  NNResultBuffer resbuf;
  GstAmlNN *self = (GstAmlNN *)data;

  ThreadInfo *pThread = &self->m_pp_thread;
  GST_INFO_OBJECT(self, "Enter, m_running=%d, m_ready=%d", pThread->m_running, pThread->m_ready);

  while (pThread->m_running) {
    g_mutex_lock(&pThread->m_mutex);
    while (!pThread->m_ready) {
      g_cond_wait(&pThread->m_cond, &pThread->m_mutex);
    }

    GST_INFO_OBJECT(self, "amlnn_post_process wait m_cond done, model=%d", self->face_det.model);
    if (!pThread->m_running) {
      g_mutex_unlock(&pThread->m_mutex);
      continue;
    }

    pThread->m_ready = FALSE;
    g_mutex_unlock(&pThread->m_mutex);

    struct timeval st;
    struct timeval ed;
    double time_total;
    gettimeofday(&st, NULL);

    // get the detect result and do post process
    DetectResult res;
    res.result.det_result.detect_num = 0;
    res.result.det_result.point = g_new(det_position_float_t, self->face_det.param.param.det_param.detect_num);
    res.result.det_result.result_name = g_new(det_classify_result_t, self->face_det.param.param.det_param.detect_num);
    det_get_inference_result(&res, self->face_det.model);
    GST_INFO_OBJECT(self, "detection result got, facenum: %d", res.result.det_result.detect_num);

    gettimeofday(&ed, NULL);
    time_total = (ed.tv_sec - st.tv_sec)*1000000.0 + (ed.tv_usec - st.tv_usec);
    st=ed;
    GST_INFO_OBJECT(self, "det_get_inference_result done, time=%lf uS", time_total);

    gint width  = self->face_det.width;
    gint height = self->face_det.height;

    resbuf.amount = res.result.det_result.detect_num;
    resbuf.results = NULL;
    if (res.result.det_result.detect_num > 0) {
      resbuf.results = g_new(NNResult, res.result.det_result.detect_num);
      for (gint i = 0; i < res.result.det_result.detect_num; i++) {
        resbuf.results[i].pos.left = res.result.det_result.point[i].point.rectPoint.left/width;
        resbuf.results[i].pos.top = res.result.det_result.point[i].point.rectPoint.top/height;
        resbuf.results[i].pos.right = res.result.det_result.point[i].point.rectPoint.right/width;
        resbuf.results[i].pos.bottom = res.result.det_result.point[i].point.rectPoint.bottom/height;
        for (gint j = 0; j < 5; j++) {
          resbuf.results[i].fpos[j].x = res.result.det_result.point[i].tpts.floatX[j]/width;
          resbuf.results[i].fpos[j].y = res.result.det_result.point[i].tpts.floatY[j]/height;
        }

        GST_INFO_OBJECT(self, "detect, id=%d, label_name=%s", res.result.det_result.result_name->label_id, res.result.det_result.result_name->label_name);
        // resbuf.results[i].label_name[0] = '\0';
        resbuf.results[i].label_id = res.result.det_result.result_name->label_id;
        snprintf(resbuf.results[i].label_name, MAX_NN_LABEL_LENGTH-1, "%s", res.result.det_result.result_name->label_name);
      }
    }

    gettimeofday(&ed, NULL);
    time_total = (ed.tv_sec - st.tv_sec)*1000000.0 + (ed.tv_usec - st.tv_usec);
    st=ed;
    GST_INFO_OBJECT(self, "nn post process done, time=%lf uS", time_total);

    g_free(res.result.det_result.point);
    g_free(res.result.det_result.result_name);

    if (pThread->m_running) {
      push_result(&self->element, &resbuf);
    }

    if (resbuf.amount > 0) {
      g_free(resbuf.results);
      resbuf.amount = 0;
      resbuf.results = NULL;
    }

    gettimeofday(&ed, NULL);
    time_total = (ed.tv_sec - st.tv_sec)*1000000.0 + (ed.tv_usec - st.tv_usec);
    GST_INFO_OBJECT(self, "detect, time=%lf uS", time_total);

#ifdef PRINT_FPS
    // calculate fps
    static int64_t frame_count = 0;
    static int64_t start = 0;
    int64_t end;
    int64_t fps = 0;
    int64_t tmp = 0;

    if (0 == start) {
      start = get_current_time_msec();
    }  else{
      /* print fps info every 100 frames */
      if ((frame_count % PRINT_FPS_INTERVAL_S == 0)) {
        end = get_current_time_msec();

        // 0.4 drop, 0.5 add 1.
        tmp = (PRINT_FPS_INTERVAL_S * 1000 * 10) / (end - start);
        fps = (PRINT_FPS_INTERVAL_S * 1000) / (end - start);
        if ((tmp % 10) >= 5) {
          fps += 1;
        }
        GST_INFO_OBJECT(self, "fps: %ld", fps);
        frame_count = 0;
        start = end;
      }
    }
    frame_count++;
#endif
  }

  GST_INFO_OBJECT(self, "exit");

  return NULL;
}



/* this function does the actual processing
 */
static GstFlowReturn gst_aml_nn_transform_ip(GstBaseTransform *base,
                                             GstBuffer *outbuf) {
  GstAmlNN *self = GST_AMLNN(base);
  GstFlowReturn ret = GST_FLOW_ERROR;

  GST_INFO_OBJECT(self, "Enter");

  if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(outbuf)))
    gst_object_sync_values(GST_OBJECT(self), GST_BUFFER_TIMESTAMP(outbuf));

  if (!self->is_info_set) {
    GST_ELEMENT_ERROR(base, CORE, NEGOTIATION, (NULL), ("unknown format"));
    return GST_FLOW_NOT_NEGOTIATED;
  }

  struct timeval st;
  struct timeval ed;
  double time_total;
  gettimeofday(&st, NULL);

  if (self->dmabuf_alloc == NULL) {
    self->dmabuf_alloc = gst_amldma_allocator_obtain("heap-gfx");
    if (self->dmabuf_alloc == NULL)
      return FALSE;
  }

  // init detect, open model, and prepare buffer, only once
  detection_init(self);

  if (self->face_det.model == DET_BUTT) {
    // face detection not enabled
    return GST_FLOW_OK;
  }

  ////////////////////////////////////////////////////////////////////////////////////
  // Prepare for detect input buffer
  // swith display buffer index
  g_mutex_lock(&self->face_det.buffer_lock);
  // if (-1 == self->face_det.prepare_idx) {
  //   self->face_det.prepare_idx = 0;
  // }

  // move prepare to next
  if (self->face_det.next_nn_idx == self->face_det.prepare_idx) {
    self->face_det.prepare_idx ++;
    if (NN_BUF_CNT == self->face_det.prepare_idx) {
      self->face_det.prepare_idx = 0;
    }
  }

  // same index, display not done, ignore this render
  if (self->face_det.cur_nn_idx == self->face_det.prepare_idx) {
    GST_INFO_OBJECT(self, "buffer is in NN, ignore this frame, prepare_idx=%d cur_nn_idx=%d",
        self->face_det.prepare_idx, self->face_det.cur_nn_idx);
      g_mutex_unlock(&self->face_det.buffer_lock);

      ret = GST_FLOW_OK;
      return ret;
  }

  int prepare_idx = self->face_det.prepare_idx;
  int next_nn_idx = self->face_det.next_nn_idx;
  g_mutex_unlock(&self->face_det.buffer_lock);

  GST_INFO_OBJECT(self, "start prepare buffer, prepare_idx=%d next_nn_idx=%d", prepare_idx, next_nn_idx);

  GstMemory *input_memory = gst_buffer_get_memory(outbuf, 0);
  if (input_memory == NULL) {
    GST_ERROR_OBJECT(self, "input buffer not valid");
    goto transform_end;
  }

  GFX_Buf inBuf;
  inBuf.format = gfx_convert_video_format(GST_VIDEO_INFO_FORMAT(&self->info));
  inBuf.size.w = self->info.width;
  inBuf.size.h = self->info.height;
  inBuf.plane_number = 1;

  gboolean is_dmabuf = gst_is_dmabuf_memory(input_memory);

  if (!is_dmabuf) {
    GST_INFO_OBJECT(self, "input is not dma buffer");

    GstMapInfo bufinfo;
    GST_INFO_OBJECT(self, "allocate memory, info.size %ld", self->info.size);

    input_memory = gst_allocator_alloc(self->dmabuf_alloc, self->info.size, NULL);
    if (input_memory == NULL) {
      GST_ERROR_OBJECT(self, "failed to allocate new dma buffer");
      goto transform_end;
    }

    GstMapInfo mapinfo;
    if (!gst_memory_map(input_memory, &mapinfo, GST_MAP_READWRITE)) {
      GST_ERROR_OBJECT(self, "failed to map memory(%p)", input_memory);
      goto transform_end;
    }
    unsigned char *pData = mapinfo.data;
    // unsigned char *pData = gst_amldmabuf_mmap(input_memory);

    if (!gst_buffer_map(outbuf, &bufinfo, GST_MAP_READ)) {
      gst_memory_unmap(input_memory, &mapinfo);
      // if (pData) gst_amldmabuf_munmap(pData, input_memory);
      GST_ERROR_OBJECT(self, "failed to map input buffer");
      goto transform_end;
    }
    memcpy(pData, bufinfo.data, self->info.size);
    gst_buffer_unmap(outbuf, &bufinfo);

    gst_memory_unmap(input_memory, &mapinfo);
    // if (pData) gst_amldmabuf_munmap(pData, input_memory);

    inBuf.fd[0] = gst_dmabuf_memory_get_fd(input_memory);
  }
  else
  {
    GST_INFO_OBJECT(self, "input is dma buffer");
    inBuf.fd[0] = gst_dmabuf_memory_get_fd(input_memory);
  }

  if (input_memory == NULL) {
    GST_ERROR_OBJECT(self, "input buffer not valid");
    goto transform_end;
  }


  GstMemory *nn_memory = self->face_det.nn_input[prepare_idx].memory;

  GFX_Buf outBuf;
  outBuf.fd[0] = self->face_det.nn_input[prepare_idx].fd;
  outBuf.format = gfx_convert_video_format(NN_INPUT_BUF_FORMAT);
  outBuf.size.w = self->face_det.width;
  outBuf.size.h = self->face_det.height;
  outBuf.plane_number = 1;

  GST_INFO_OBJECT(self, "prepare detect prepare_idx=%d, fd=%d", prepare_idx, outBuf.fd[0]);

  GFX_Rect inRect = {0, 0, self->info.width, self->info.height};
  GFX_Rect outRect = {0, 0, self->face_det.width, self->face_det.height};

  // Convert source buffer to detect buffer
  gfx_stretchblit(self->m_gfxhandle,
                &inBuf, &inRect,
                &outBuf, &outRect,
                GFX_AML_ROTATION_0,
                1);

  gettimeofday(&ed, NULL);
  time_total = (ed.tv_sec - st.tv_sec)*1000000.0 + (ed.tv_usec - st.tv_usec);
  st = ed;
  GST_INFO_OBJECT(self, "gfx_stretchblit done, time=%lf uS", time_total);

  //set input data to npu : yi.zhang1 add
//   GstMapInfo mapinfo;
//   if (!gst_memory_map(self->face_det.nn_input[prepare_idx].memory, &mapinfo, GST_MAP_READWRITE)) {
//     GST_ERROR_OBJECT(self, "failed to map memory(%p)", self->face_det.nn_input[prepare_idx].memory);
//     return FALSE;
//   }

//   unsigned char *pData = mapinfo.data;
//   // unsigned char *pData = gst_amldmabuf_mmap(self->face_det.nn_input[nn_idx].memory);

//   GST_INFO_OBJECT(self, "nn_idx=%d, rowbytes=%d, stride=%d",
//     prepare_idx, self->face_det.rowbytes, self->face_det.stride);

//   if (self->face_det.rowbytes != self->face_det.stride) {
//     gint rowbytes = self->face_det.rowbytes;
//     gint stride = self->face_det.stride;
//     for (int i = 0; i < self->face_det.height; i++) {
//       memcpy(&pData[rowbytes * i], &pData[stride * i], rowbytes);
//     }
//   }

//   GST_INFO_OBJECT(self, "pData=%p, width=%d, height=%d, channel=%d",
//     pData, self->face_det.width, self->face_det.height, self->face_det.channel);

//   input_image_t im;
//   im.data = pData;
//   im.pixel_format = PIX_FMT_RGB888;
//   im.width = self->face_det.width;
//   im.height = self->face_det.height;
//   im.channel = self->face_det.channel;
//   det_status_t rc = det_set_data_to_NPU(im, self->face_det.model);
//   gst_memory_unmap(self->face_det.nn_input[prepare_idx].memory, &mapinfo);

//   gettimeofday(&ed, NULL);
//   time_total = (ed.tv_sec - st.tv_sec)*1000000.0 + (ed.tv_usec - st.tv_usec);
//   st = ed;
//   GST_INFO_OBJECT(self, "det_set_data_to_NPU done, time=%lf uS", time_total);


  // update new render buffer
  g_mutex_lock(&self->face_det.buffer_lock);
  self->face_det.next_nn_idx = self->face_det.prepare_idx;
  g_mutex_unlock(&self->face_det.buffer_lock);



  ThreadInfo *pThread = &self->m_nn_thread;
  if (g_mutex_trylock(&pThread->m_mutex)) {
    // skip the following transform
    // gst_base_transform_set_passthrough(base, TRUE);

    pThread->m_ready = TRUE;
    g_cond_signal(&pThread->m_cond);
    GST_INFO_OBJECT(self, "gst_aml_nn_transform_ip send cond");
    g_mutex_unlock(&pThread->m_mutex);
  }

  ret = GST_FLOW_OK;

#ifdef PRINT_FPS
    // calculate fps
    static int64_t frame_count = 0;
    static int64_t start = 0;
    int64_t end;
    int64_t fps = 0;
    int64_t tmp = 0;

    if (0 == start) {
      start = get_current_time_msec();
    }  else{
      /* print fps info every 100 frames */
      if ((frame_count % PRINT_FPS_INTERVAL_S == 0)) {
        end = get_current_time_msec();

        // 0.4 drop, 0.5 add 1.
        tmp = (PRINT_FPS_INTERVAL_S * 1000 * 10) / (end - start);
        fps = (PRINT_FPS_INTERVAL_S * 1000) / (end - start);
        if ((tmp % 10) >= 5) {
          fps += 1;
        }
        GST_INFO_OBJECT(self, "fps: %ld", fps);
        frame_count = 0;
        start = end;
      }
    }
    frame_count++;
#endif

transform_end:

  if (NULL != input_memory)
  {
    gst_memory_unref(input_memory);
    input_memory = NULL;
  }

  GST_INFO_OBJECT(self, "Leave, signal done");

  return ret;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean amlnn_init(GstPlugin *amlnn) {
  GST_DEBUG_CATEGORY_INIT(gst_aml_nn_debug, "amlnn", 0, "amlogic nn element");

  return gst_element_register(amlnn, "amlnn", GST_RANK_PRIMARY, GST_TYPE_AMLNN);
}

/* gstreamer looks for this structure to register amlnns
 *
 */
GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, amlnn,
                  "amlogic nn plugins", amlnn_init, VERSION, "LGPL",
                  "amlogic nn plugins", "http://openlinux.amlogic.com")