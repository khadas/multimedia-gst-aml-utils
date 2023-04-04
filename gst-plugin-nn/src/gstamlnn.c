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
 * gst-launch -v -m fakesrc ! amlnn ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gmodule.h>
#include <gst/allocators/gstamlionallocator.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "facedb.h"
#include "jpeg.h"

#include "gstamlnn.h"

#include "gfx_2d.h"
#include "gst_ge2d.h"

// for test time
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


// use fake detect
//#define USE_FAKE_DETECT


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

#define DEFAULT_PROP_FACE_RECOG_MODEL DET_AML_FACE_RECOGNITION
#define DEFAULT_PROP_FORMAT "uid,name"
#define DEFAULT_PROP_DBPATH ""
#define DEFAULT_PROP_THRESHOLD 0.6
#define DEFAULT_PROP_RECOG_TRIGGER_TIMEOUT 2
#define DEFAULT_PROP_MAX_DET_NUM 10


#define NN_INPUT_BUF_FORMAT GST_VIDEO_FORMAT_RGB

/* Filter signals and args */
enum {
  SIGNAL_FACE_RECOGNIZED,
  LAST_SIGNAL
};

static guint gst_amlnn_signals[LAST_SIGNAL] = { 0 };

enum {
  PROP_0,
  PROP_FACE_DET_MODEL,
  PROP_FACE_RECOG_MODEL,
  PROP_DBPATH,
  PROP_FORMAT,
  PROP_THRESHOLD,
  PROP_RECOG_TRIGGER_TIMEOUT,
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

#define GST_TYPE_AML_FACE_RECOG_MODEL (gst_aml_face_recog_model_get_type())
static GType gst_aml_face_recog_model_get_type(void) {
  static GType aml_face_recog_model = 0;
  static const GEnumValue aml_face_recog_models[] = {
      {DET_FACENET, "facenet", "facenet"},
      {DET_AML_FACE_RECOGNITION, "aml_face_recognition", "aml_face_recognition"},
      {DET_BUTT, "disable", "disable"},
      {0, NULL, NULL},
  };

  if (!aml_face_recog_model) {
    aml_face_recog_model =
        g_enum_register_static("GstAMLFaceRecogModel", aml_face_recog_models);
  }
  return aml_face_recog_model;
}

/* the capabilities of the inputs and outputs.
 */
#define GST_VIDEO_FORMATS                                                      \
  "{"                                                                          \
  " RGBA, RGBx, RGB, "                                                          \
  " BGRA, BGR, "                                                                \
  " YV12, NV16, NV21, UYVY, NV12,"                                             \
  " I420"                                                                      \
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

static gboolean gst_aml_nn_sink_event(GstBaseTransform *base, GstEvent *event);

static gboolean detection_init(GstAmlNN *self);
static gpointer amlnn_process(void *data);
static void push_result(GstBaseTransform *base, NNResultBuffer *resbuf);
/* GObject vmethod implementations */


/* initialize the amlnn's class */
static void gst_aml_nn_class_init(GstAmlNNClass *klass) {
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;

  gobject_class->set_property = gst_aml_nn_set_property;
  gobject_class->get_property = gst_aml_nn_get_property;
  gobject_class->finalize = gst_aml_nn_finalize;

  gst_amlnn_signals[SIGNAL_FACE_RECOGNIZED] = g_signal_new(
      "face-recognized", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);

  g_object_class_install_property(
      G_OBJECT_CLASS(klass), PROP_FACE_DET_MODEL,
      g_param_spec_enum("detection-model", "detection-model",
                        "face detection model", GST_TYPE_AML_FACE_DET_MODEL,
                        DEFAULT_PROP_FACE_DET_MODEL,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      G_OBJECT_CLASS(klass), PROP_FACE_RECOG_MODEL,
      g_param_spec_enum("recognition-model", "recognition-model",
                        "face recognition model", GST_TYPE_AML_FACE_RECOG_MODEL,
                        DEFAULT_PROP_FACE_RECOG_MODEL,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      G_OBJECT_CLASS(klass), PROP_DBPATH,
      g_param_spec_string(
          "db-path", "db-path", "database location of face recognition",
          DEFAULT_PROP_DBPATH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      G_OBJECT_CLASS(klass), PROP_FORMAT,
      g_param_spec_string("result-format", "result-format",
                          "string format of face recognition result",
                          DEFAULT_PROP_FORMAT,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      gobject_class, PROP_THRESHOLD,
      g_param_spec_float(
          "threshold", "Threshold", "threshold of face recognition", 0.01, 1.50,
          DEFAULT_PROP_THRESHOLD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      gobject_class, PROP_RECOG_TRIGGER_TIMEOUT,
      g_param_spec_int(
          "recog-trigger-timeout", "Recognition-Trigger-Timeout", "timeout of recognition trigger for single face (seconds)", 1, 60,
          DEFAULT_PROP_RECOG_TRIGGER_TIMEOUT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      gobject_class, PROP_MAX_DET_NUM,
      g_param_spec_int(
          "max-detect-num", "max-detection-number", "maximum detection number", 10, 230,
          DEFAULT_PROP_MAX_DET_NUM, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_details_simple(gstelement_class, "amlnn",
                                       "Generic/Filter", "Amlogic NN module",
                                       "Jemy Zhang <jun.zhang@amlogic.com>");

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

  GST_BASE_TRANSFORM_CLASS(klass)->sink_event =
      GST_DEBUG_FUNCPTR(gst_aml_nn_sink_event);

  GST_BASE_TRANSFORM_CLASS(klass)->transform_ip_on_passthrough = FALSE;
}

/* initialize the new element
 * initialize instance structure
 */
static void gst_aml_nn_init(GstAmlNN *nn) {
  nn->is_info_set = FALSE;

  memset(&nn->face_det, 0, sizeof(ModelInfo));
  nn->face_det.model = DEFAULT_PROP_FACE_DET_MODEL;
  nn->max_detect_num = DEFAULT_PROP_MAX_DET_NUM;
  memset(&nn->face_recog, 0, sizeof(ModelInfo));
  nn->face_recog.model = DEFAULT_PROP_FACE_RECOG_MODEL;

  nn->db_param.handle = NULL;
  nn->db_param.file = g_strdup(DEFAULT_PROP_DBPATH);
  nn->db_param.format = g_strdup(DEFAULT_PROP_FORMAT);
  nn->db_param.threshold = DEFAULT_PROP_THRESHOLD;
  nn->db_param.bstore_face = FALSE;
  nn->custimg = NULL;

  nn->handle = NULL;

  g_cond_init(&nn->m_cond);
  g_mutex_init(&nn->m_mutex);
  g_mutex_init(&nn->face_det.buffer_lock);

  nn->face_det.prepare_idx = -1;
  nn->face_det.cur_nn_idx = -1;
  nn->face_det.next_nn_idx = -1;

  nn->m_ready = FALSE;

  list_init(&nn->recognized_list);
  nn->recog_trigger_timeout = DEFAULT_PROP_RECOG_TRIGGER_TIMEOUT;

  // set debug log level
  det_set_log_config(DET_DEBUG_LEVEL_WARN,DET_LOG_TERMINAL);

  // init DMA
  if (nn->dmabuf_alloc == NULL) {
    nn->dmabuf_alloc = gst_amlion_allocator_obtain();
  }
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

#ifdef USE_FAKE_DETECT
  //TODO : fleet temp
  m->width = 640;
  m->height = 384;
  m->channel = 3;
#endif

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

static gboolean open_db(RecogDBParam *param) {
  if (param->handle != NULL)
    return TRUE;

  if (param->file[0] != '\0') {
    param->handle = db_init(param->file);
    db_set_threshold(param->threshold);
  }
  return param->handle != NULL;
}

static gboolean close_db(RecogDBParam *param) {
  if (param->handle == NULL)
    return TRUE;
  db_deinit(param->handle);
  param->handle = NULL;
  return TRUE;
}

struct idle_task_data {
  GstAmlNN *self;
  union {
    struct _model {
      ModelInfo *minfo;
      det_model_type new_model;
    } model;
    struct _db {
      gchar *file;
    } db;
    struct _img {
      gchar *file;
    } img;
  } u;
};

static gboolean idle_close_model(struct idle_task_data *data) {
  if (data == NULL || data->self == NULL || data->self->m_running == FALSE ||
      data->u.model.minfo == NULL) {
    return G_SOURCE_REMOVE;
  }

  GstAmlNN *self = data->self;
  ModelInfo *minfo = data->u.model.minfo;

  g_mutex_lock(&self->m_mutex);
  // close detect model for the next reinitialization
  close_model(minfo);
  minfo->model = data->u.model.new_model;
  if (data->u.model.new_model == DET_BUTT) {
    // notify the overlay to clear info
    push_result (&self->element, NULL);
  }
  g_mutex_unlock(&self->m_mutex);

  g_free(data);
  return G_SOURCE_REMOVE;
}

static gboolean idle_close_db(struct idle_task_data *data) {
  if (data == NULL || data->self == NULL || data->self->m_running == FALSE ||
      data->u.db.file == NULL) {
    return G_SOURCE_REMOVE;
  }

  GstAmlNN *self = data->self;

  g_mutex_lock(&self->m_mutex);
  close_db(&self->db_param);
  g_free(self->db_param.file);
  self->db_param.file = data->u.db.file;
  g_mutex_unlock(&self->m_mutex);

  g_free(data);
  return G_SOURCE_REMOVE;
}

static gboolean idle_request_capface(struct idle_task_data *data) {
  if (data == NULL || data->self == NULL || data->self->m_running == FALSE) {
    return G_SOURCE_REMOVE;
  }

  GstAmlNN *self = data->self;

  g_mutex_lock(&self->m_mutex);
  self->db_param.bstore_face = TRUE;
  g_mutex_unlock(&self->m_mutex);

  g_free(data);
  return G_SOURCE_REMOVE;
}

static gboolean idle_request_capface_from_image(struct idle_task_data *data) {
  if (data == NULL || data->self == NULL || data->self->m_running == FALSE ||
      data->u.img.file == NULL) {
    return G_SOURCE_REMOVE;
  }

  GstAmlNN *self = data->self;

  g_mutex_lock(&self->m_mutex);
  if (self->custimg) g_free(self->custimg);
  self->custimg = data->u.img.file;
  g_mutex_unlock(&self->m_mutex);

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
  case PROP_FACE_RECOG_MODEL: {
    det_model_type m = g_value_get_enum(value);
    if (m != self->face_recog.model) {
      if (self->face_recog.initialized) {
        struct idle_task_data *data = g_new(struct idle_task_data, 1);
        data->self = self;
        data->u.model.minfo = &self->face_recog;
        data->u.model.new_model = m;
        // close recognition model for the next reinitialization
        g_idle_add((GSourceFunc)idle_close_model, data);
      } else {
        self->face_recog.model = m;
      }
    }
  } break;
  case PROP_DBPATH: {
    gchar *file = g_value_dup_string(value);
    if (g_strcmp0(file, self->db_param.file)) {
      if (self->db_param.handle != NULL) {
        struct idle_task_data *data = g_new(struct idle_task_data, 1);
        data->self = self;
        data->u.db.file = file;
        g_idle_add((GSourceFunc)idle_close_db, data);
      } else {
        self->db_param.file = file;
      }
    } else {
      g_free(file);
    }
  } break;
  case PROP_FORMAT:
    g_free(self->db_param.format);
    self->db_param.format = g_value_dup_string(value);
    break;
  case PROP_THRESHOLD:
    self->db_param.threshold = g_value_get_float(value);
    db_set_threshold(self->db_param.threshold);
    break;
  case PROP_RECOG_TRIGGER_TIMEOUT:
    self->recog_trigger_timeout = g_value_get_int(value);
    break;
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
  case PROP_FACE_RECOG_MODEL:
    g_value_set_enum(value, self->face_recog.model);
    break;
  case PROP_DBPATH:
    g_value_set_string(value, self->db_param.file);
    break;
  case PROP_FORMAT:
    g_value_set_string(value, self->db_param.format);
    break;
  case PROP_THRESHOLD:
    g_value_set_float(value, self->db_param.threshold);
    break;
  case PROP_RECOG_TRIGGER_TIMEOUT:
    g_value_set_int(value, self->recog_trigger_timeout);
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

  det_set_log_config(DET_DEBUG_LEVEL_ERROR, DET_LOG_TERMINAL);

  self->handle = gfx_init();
  if (self->handle == NULL) {
    GST_ERROR_OBJECT(self, "failed to initialize gfx2d");
    return FALSE;
  }

  self->m_running = TRUE;
  self->m_thread = g_thread_new("nn process", amlnn_process, self);
  return TRUE;
}

static gboolean gst_aml_nn_close(GstBaseTransform *base) {
  GstAmlNN *self = GST_AMLNN(base);

  GST_DEBUG_OBJECT(self, "closing, waiting for lock");
  self->m_running = FALSE;
  g_mutex_lock(&self->m_mutex);
  self->m_ready = TRUE;
  g_cond_signal(&self->m_cond);
  g_mutex_unlock(&self->m_mutex);

  g_thread_join(self->m_thread);
  self->m_thread = NULL;

  if (self->custimg) {
    g_free(self->custimg);
    self->custimg = NULL;
  }

  if (self->handle) {
    gfx_deinit(self->handle);
    self->handle = NULL;
  }

  GST_DEBUG_OBJECT(self, "closed");

  return TRUE;
}

#define FREE_STRING(s)                                                         \
  do {                                                                         \
    if (s) {                                                                   \
      g_free(s);                                                               \
      s = NULL;                                                                \
    }                                                                          \
  } while (0)

static void gst_aml_nn_finalize(GObject *object) {
  GstAmlNN *self = GST_AMLNN(object);
  FREE_STRING(self->db_param.file);
  FREE_STRING(self->db_param.format);
  G_OBJECT_CLASS(parent_class)->finalize(object);
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

/* GstBaseTransform vmethod implementations */
static gboolean gst_aml_nn_sink_event(GstBaseTransform *base, GstEvent *event) {
  GstAmlNN *self = GST_AMLNN(base);

  switch (GST_EVENT_TYPE(event)) {
  case GST_EVENT_CUSTOM_DOWNSTREAM_OOB: {
    const GstStructure *st = gst_event_get_structure(event);

    // GST_INFO_OBJECT(self, "GST_EVENT_CUSTOM_DOWNSTREAM_OOB");

    if (gst_structure_has_name(st, "do-image-facecap")) {

      GST_INFO_OBJECT(self, "do-image-facecap");

      const GValue *value = gst_structure_get_value(st, "image-path");
      struct idle_task_data *data = g_new(struct idle_task_data, 1);
      data->self = self;
      data->u.img.file = g_value_dup_string (value);
      g_idle_add((GSourceFunc)idle_request_capface_from_image, data);
    }
    if (gst_structure_has_name(st, "do-facecap")) {

      GST_INFO_OBJECT(self, "do-facecap");

      struct idle_task_data *data = g_new(struct idle_task_data, 1);
      data->self = self;
      g_idle_add((GSourceFunc)idle_request_capface, data);
    }
  } break;
  default:
    break;
  }
  return GST_BASE_TRANSFORM_CLASS(parent_class)->sink_event(base, event);
}

static void push_result(GstBaseTransform *base,
                        NNResultBuffer *resbuf) {
  GstMapInfo info;
  GstAmlNN *self = GST_AMLNN(base);

  if (resbuf == NULL || resbuf->amount <= 0) {
    GST_INFO_OBJECT(self, "nn-result-clear");
    GstStructure *st = gst_structure_new("nn-result-clear", NULL, NULL);
    GstEvent *event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, st);
    gst_element_send_event(&base->element, event);
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
  }
  gst_buffer_unref(gstbuf);
}

static GstMemory *process_custom_image(GstAmlNN *self) {
  int width, height, stride;
  GstMemory *input_memory = NULL;
  GstMapInfo minfo;

  // read image to input memory
  GST_INFO_OBJECT(self, "processing image: %s", self->custimg);
  if (!jpeg_to_rgb888(self->custimg, &width, &height, &stride, NULL)) {
    goto fail_exit;
  }

  gint input_size = stride * height;
  input_memory = gst_allocator_alloc(self->dmabuf_alloc, input_size, NULL);
  if (input_memory == NULL) {
    GST_ERROR_OBJECT(self, "failed to allocate new dma buffer");
    goto fail_exit;
  }
  if (!gst_memory_map(input_memory, &minfo, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT(self, "failed to map new dma buffer");
    goto fail_exit;
  }

  if (!jpeg_to_rgb888(self->custimg, &width, &height, &stride, minfo.data)) {
    GST_ERROR_OBJECT(self, "failed to generate rgb data from %s",
                     self->custimg);
    gst_memory_unmap(input_memory, &minfo);
    goto fail_exit;
  }
  gst_memory_unmap(input_memory, &minfo);

  GST_INFO_OBJECT(self, "image size: %dx%d", width, height);

fail_exit:
  if (input_memory) {
    gst_memory_unref (input_memory);
  }
  return input_memory;
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

      self->face_det.nn_input[i].memory = gst_allocator_alloc(
          self->dmabuf_alloc,
          self->face_det.stride * self->face_det.height, NULL);
      if (NULL == self->face_det.nn_input[i].memory) {
        GST_ERROR_OBJECT(self, "failed to allocate the nn_input dma buffer, %p, %d",
          self->dmabuf_alloc, self->face_det.stride * self->face_det.height);
        return FALSE;
      }

      self->face_det.nn_input[i].fd = gst_dmabuf_memory_get_fd(self->face_det.nn_input[i].memory);

      GST_INFO_OBJECT(self, "[%d]dmabuf_alloc=%p, stride=%d, height=%d, memory=%p", i,
          self->dmabuf_alloc, self->face_det.stride, self->face_det.height, self->face_det.nn_input[i].memory);
    }
  }

  return TRUE;
}



static gboolean detection_process(GstAmlNN *self,
                                  NNResultBuffer *resbuf) {
  gboolean ret = TRUE;

  GST_INFO_OBJECT(self, "Enter");

  struct timeval st;
  struct timeval ed;
  double time_total;
  gettimeofday(&st, NULL);

    // get display buffer
  g_mutex_lock(&self->face_det.buffer_lock);
  int nn_idx = self->face_det.next_nn_idx;
  self->face_det.cur_nn_idx = nn_idx;
  g_mutex_unlock(&self->face_det.buffer_lock);

  GstMapInfo minfo;
  if (!gst_memory_map(self->face_det.nn_input[nn_idx].memory, &minfo, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT(self, "failed to map output detection buffer");
    return FALSE;
  }

  GST_INFO_OBJECT(self, "nn_idx=%d, rowbytes=%d, stride=%d",
    nn_idx, self->face_det.rowbytes, self->face_det.stride);

  if (self->face_det.rowbytes != self->face_det.stride) {
    gint rowbytes = self->face_det.rowbytes;
    gint stride = self->face_det.stride;
    for (int i = 0; i < self->face_det.height; i++) {
      memcpy(&minfo.data[rowbytes * i], &minfo.data[stride * i], rowbytes);
    }
  }

  GST_INFO_OBJECT(self, "minfo.data=%p, width=%d, height=%d, channel=%d",
    minfo.data, self->face_det.width, self->face_det.height, self->face_det.channel);

  input_image_t im;
  im.data = minfo.data;
  im.pixel_format = PIX_FMT_RGB888;
  im.width = self->face_det.width;
  im.height = self->face_det.height;
  im.channel = self->face_det.channel;
  det_status_t rc = det_set_input(im, self->face_det.model);
  gst_memory_unmap(self->face_det.nn_input[nn_idx].memory, &minfo);

  if (rc != DET_STATUS_OK) {
    GST_ERROR_OBJECT(self, "failed to set input to detection model");
    return FALSE;
  }

  GST_INFO_OBJECT(self, "waiting for detection result");

#ifndef USE_FAKE_DETECT
  DetectResult res;
  res.result.det_result.detect_num = 0;
  res.result.det_result.point = g_new(det_position_float_t, self->face_det.param.param.det_param.detect_num);
  res.result.det_result.result_name = g_new(det_classify_result_t, self->face_det.param.param.det_param.detect_num);
  det_get_result(&res, self->face_det.model);
  GST_INFO_OBJECT(self, "detection result got, facenum: %d", res.result.det_result.detect_num);

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
  self->face_det.cur_nn_idx = -1;
  g_mutex_unlock(&self->face_det.buffer_lock);

  resbuf->amount = res.result.det_result.detect_num;
  resbuf->results = NULL;
  if (res.result.det_result.detect_num > 0) {
    resbuf->results = g_new(NNResult, res.result.det_result.detect_num);
    for (gint i = 0; i < res.result.det_result.detect_num; i++) {
      resbuf->results[i].pos.left = res.result.det_result.point[i].point.rectPoint.left/im.width;
      resbuf->results[i].pos.top = res.result.det_result.point[i].point.rectPoint.top/im.height;
      resbuf->results[i].pos.right = res.result.det_result.point[i].point.rectPoint.right/im.width;
      resbuf->results[i].pos.bottom = res.result.det_result.point[i].point.rectPoint.bottom/im.height;
      for (gint j = 0; j < 5; j++) {
        resbuf->results[i].fpos[j].x = res.result.det_result.point[i].tpts.floatX[j]/im.width;
        resbuf->results[i].fpos[j].y = res.result.det_result.point[i].tpts.floatY[j]/im.height;
      }

      GST_INFO_OBJECT(self, "detect, id=%d, label_name=%s", res.result.det_result.result_name->label_id, res.result.det_result.result_name->label_name);
      // resbuf->results[i].label_name[0] = '\0';
      resbuf->results[i].label_id = res.result.det_result.result_name->label_id;
      snprintf(resbuf->results[i].label_name, MAX_NN_LABEL_LENGTH-1, "%s", res.result.det_result.result_name->label_name);
    }
  }

  g_free(res.result.det_result.point);
  g_free(res.result.det_result.result_name);

  gettimeofday(&ed, NULL);
  time_total = (ed.tv_sec - st.tv_sec)*1000000.0 + (ed.tv_usec - st.tv_usec);

  GST_INFO_OBJECT(self, "detect, time=%lf uS", time_total);

#else

  int x_rand;
  int y_rand;
  int w_rand;

  DetectResult res;
  res.result.det_result.detect_num = 200;
  res.result.det_result.point = g_new(det_position_float_t, self->face_det.param.param.det_param.detect_num);
  res.result.det_result.result_name = g_new(amlnn_processdet_classify_result_t, self->face_det.param.param.det_param.detect_num);
  det_get_result(&res, self->face_det.model);
  GST_INFO_OBJECT(self, "detection result got, facenum: %d, maxfacenum: %d",
    res.result.det_result.detect_num, self->face_det.param.param.det_param.detect_num);

  // fleet temp
  resbuf->amount = res.result.det_result.detect_num;
  resbuf->results = g_new(NNResult, resbuf->amount);

  static struct timeval timePrev;
  struct timeval timeCurrent;
  double time_us;
  gettimeofday(&timeCurrent, NULL);
  time_us = (timeCurrent.tv_sec - timePrev.tv_sec)*1000000.0 + (timeCurrent.tv_usec - timePrev.tv_usec);
  timePrev=timeCurrent;

  unsigned seed = (unsigned)time_us;

  for (gint i = 0; i < resbuf->amount; i++) {
    srand((unsigned)seed+i);
    x_rand = rand() % 1000;
    y_rand = rand() % 1000;
    w_rand = rand() % 100 + 50;

    resbuf->results[i].pos.left = 0.001*x_rand;
    resbuf->results[i].pos.top = 0.001*y_rand;
    resbuf->results[i].pos.right = 0.001*w_rand+resbuf->results[i].pos.left;
    resbuf->results[i].pos.bottom = 0.002*w_rand+resbuf->results[i].pos.top;
    resbuf->results[i].info[0] = '\0';
  }

  GST_INFO_OBJECT(self, "fake detection result");

#endif

  GST_INFO_OBJECT(self, "Leave");
  return ret;
}



static gpointer amlnn_process(void *data) {
  NNResultBuffer resbuf;
  GstAmlNN *self = (GstAmlNN *)data;
  GstMemory *input_memory = NULL;
  gboolean is_normal_process = TRUE;

  GST_INFO_OBJECT(self, "Enter, m_running=%d, m_ready=%d", self->m_running, self->m_ready);

  while (self->m_running) {
    g_mutex_lock(&self->m_mutex);
    while (!self->m_ready) {
      g_cond_wait(&self->m_cond, &self->m_mutex);
    }

    GST_INFO_OBJECT(self, "wait m_cond done, model=%d", self->face_det.model);

    if (!self->m_running) {
      goto loop_continue;
    }

    self->m_ready = FALSE;
    g_mutex_unlock(&self->m_mutex);

    if (self->face_det.model == DET_BUTT) {
      // face detection not enabled,
      // ignore the request and exit
      GST_DEBUG_OBJECT(self, "face detection model disabled");
      // make sure the input memory would be released
      goto loop_continue;
    }

    if (self->custimg) {
      GST_INFO_OBJECT(self, "custimg=%s", self->custimg);
      process_custom_image(self);
      g_free(self->custimg);
      self->custimg = NULL;
      self->db_param.bstore_face = TRUE;
      is_normal_process = FALSE;
    } else{
      is_normal_process = TRUE;
      GST_INFO_OBJECT(self, "is_normal_process=%d", is_normal_process);
    }

    detection_process(self, &resbuf);

    if (is_normal_process && self->m_running) {
      push_result(&self->element, &resbuf);
    }

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

  loop_continue:
    if (resbuf.amount > 0) {
      g_free(resbuf.results);
      resbuf.amount = 0;
      resbuf.results = NULL;
    }
    if (input_memory) {
      gst_memory_unref (input_memory);
      input_memory = NULL;
    }

    // continue process buffer
    // if (self->m_running) {
    //   gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(self), FALSE);
    // }
    // self->m_ready = FALSE;
    // g_mutex_unlock(&self->m_mutex);
  }

  // exiting
  close_db(&self->db_param);
  close_model(&self->face_det);
  close_model(&self->face_recog);

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
  gboolean is_dmabuf = gst_is_dmabuf_memory(input_memory);

  if (self->dmabuf_alloc == NULL) {
    self->dmabuf_alloc = gst_amlion_allocator_obtain();
  }

  struct timeval st;
  struct timeval ed;
  double time_total;
  gettimeofday(&st, NULL);

  if (!is_dmabuf) {
    GstMapInfo minfo, bufinfo;

    gst_memory_unref(input_memory);

    GST_INFO_OBJECT(self, "allocate memory, info.size %ld", self->info.size);

    input_memory = gst_allocator_alloc(self->dmabuf_alloc, self->info.size, NULL);
    if (input_memory == NULL) {
      GST_ERROR_OBJECT(self, "failed to allocate new dma buffer");
      goto transform_end;
    }
    if (!gst_memory_map(input_memory, &minfo, GST_MAP_WRITE)) {
      GST_ERROR_OBJECT(self, "failed to map new dma buffer");
      goto transform_end;
    }
    if (!gst_buffer_map(outbuf, &bufinfo, GST_MAP_READ)) {
      gst_memory_unmap(input_memory, &minfo);
      GST_ERROR_OBJECT(self, "failed to map input buffer");
      goto transform_end;
    }
    memcpy(minfo.data, bufinfo.data, self->info.size);
    gst_buffer_unmap(outbuf, &bufinfo);
    gst_memory_unmap(input_memory, &minfo);
  }

  if (input_memory == NULL) {
    GST_ERROR_OBJECT(self, "input buffer not valid");
    goto transform_end;
  }

  GFX_Buf inBuf;
  inBuf.fd = gst_dmabuf_memory_get_fd(input_memory);
  inBuf.format = gfx_convert_video_format(GST_VIDEO_INFO_FORMAT(&self->info));
  inBuf.is_ionbuf = gst_is_amlionbuf_memory(input_memory);
  inBuf.size.w = self->info.width;
  inBuf.size.h = self->info.height;

  GstMemory *nn_memory = self->face_det.nn_input[prepare_idx].memory;

  GFX_Buf outBuf;
  outBuf.fd = self->face_det.nn_input[prepare_idx].fd;
  outBuf.format = gfx_convert_video_format(NN_INPUT_BUF_FORMAT);
  outBuf.is_ionbuf = gst_is_amlionbuf_memory(nn_memory);
  outBuf.size.w = self->face_det.width;
  outBuf.size.h = self->face_det.height;

  GST_INFO_OBJECT(self, "prepare detect memory=%p, fd=%d", nn_memory, outBuf.fd);

  GFX_Rect inRect = {0, 0, self->info.width, self->info.height};
  GFX_Rect outRect = {0, 0, self->face_det.width, self->face_det.height};

  // Convert source buffer to detect buffer
  gfx_stretchblit(self->handle,
                &inBuf, &inRect,
                &outBuf, &outRect,
                GFX_AML_ROTATION_0,
                1);

  // update new render buffer
  g_mutex_lock(&self->face_det.buffer_lock);
  self->face_det.next_nn_idx = self->face_det.prepare_idx;
  g_mutex_unlock(&self->face_det.buffer_lock);

  gettimeofday(&ed, NULL);
  time_total = (ed.tv_sec - st.tv_sec)*1000000.0 + (ed.tv_usec - st.tv_usec);
  GST_INFO_OBJECT(self, "gfx_stretchblit done, time=%lf uS", time_total);

  if (g_mutex_trylock(&self->m_mutex)) {
    // skip the following transform
    // gst_base_transform_set_passthrough(base, TRUE);

    self->m_ready = TRUE;
    g_cond_signal(&self->m_cond);
    g_mutex_unlock(&self->m_mutex);
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

  if (input_memory != NULL) {
    gst_memory_unref(input_memory);
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