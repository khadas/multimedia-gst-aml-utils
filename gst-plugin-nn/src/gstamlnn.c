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
#include "imgproc.h"
#include "jpeg.h"

#include "gstamlnn.h"


// temp fleet for test
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


// use fake detect
//#define USE_FAKE_DETECT


GST_DEBUG_CATEGORY_STATIC(gst_aml_nn_debug);
#define GST_CAT_DEFAULT gst_aml_nn_debug

struct relative_pos {
  float left;
  float top;
  float right;
  float bottom;
};

struct facepoint_pos {
  float x;
  float y;
};

#define FACE_INFO_BUFSIZE 1024
struct nn_result {
  struct relative_pos pos;
  struct facepoint_pos fpos[5];
  char info[FACE_INFO_BUFSIZE];
};

struct nn_result_buffer {
  gint amount; // amount of result
  struct nn_result *results;
};

struct recognized_info {
  struct listnode list;
  gint uid;
  gchar *name;
  GstClockTime timestamp;
};

//#define DEFAULT_PROP_FACE_DET_MODEL DET_AML_FACE_DETECTION
#define DEFAULT_PROP_FACE_DET_MODEL DET_YOLO_V3

#define DEFAULT_PROP_FACE_RECOG_MODEL DET_AML_FACE_RECOGNITION
#define DEFAULT_PROP_FORMAT "uid,name"
#define DEFAULT_PROP_DBPATH ""
#define DEFAULT_PROP_THRESHOLD 0.6
#define DEFAULT_PROP_RECOG_TRIGGER_TIMEOUT 2
#define DEFAULT_PROP_MAX_DET_NUM 10

#define NN_INPUT_BUF_W 1280
#define NN_INPUT_BUF_H 720
#define NN_INPUT_BUF_FORMAT GST_VIDEO_FORMAT_RGB
#define NN_INPUT_BUF_STRIDE NN_INPUT_BUF_W * 3
#define NN_INPUT_BUF_SIZE NN_INPUT_BUF_STRIDE * NN_INPUT_BUF_H

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
  static const GEnumValue aml_face_det_models[] = {
      {DET_YOLOFACE_V2, "yoloface-v2", "yoloface v2"},
      {DET_MTCNN_V1, "mtcnn-v1", "mtcnn v1"},
      {DET_MTCNN_V2, "mtcnn-v2", "mtcnn v2"},
      {DET_AML_FACE_DETECTION, "aml_face_detection", "aml_face_detection"},
      {DET_BUTT, "disable", "disable"},
      //    {DET_YOLO_V2, "yolo-v2", "yolo v2"},
      //    {DET_YOLO_V3, "yolo-v3", "yolo v3"},
      //    {DET_YOLO_TINY, "yolo-tiny", "yolo tiny"},
      //    {DET_SSD, "ssd", "ssd"},
      //    {DET_FASTER_RCNN, "faster-rcnn", "faster rcnn"},
      //    {DET_DEEPLAB_V1, "deeplab-v1", "deeplab v1"},
      //    {DET_DEEPLAB_V2, "deeplab-v2", "deeplab v2"},
      //    {DET_DEEPLAB_V3, "deeplab-v3", "deeplab v3"},
      {0, NULL, NULL},
  };

  if (!aml_face_det_model) {
    aml_face_det_model =
        g_enum_register_static("GstAMLFaceDetectModel", aml_face_det_models);
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
  " RGB, RGBA, RGBx,"                                                          \
  " BGR, BGRA,"                                                                \
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

static gpointer amlnn_process(void *data);
static void push_result(GstBaseTransform *base, struct nn_result_buffer *resbuf);
/* GObject vmethod implementations */

static void trigger_recognized(GstAmlNN *self, gint uid, gchar* name) {
  struct listnode *pos, *q;
  gboolean found = FALSE;
  gboolean trigger = FALSE;
  struct timeval tv;
  gettimeofday(&tv, 0);
  GstClockTime now = GST_TIMEVAL_TO_TIME(tv);
  if (!list_empty(&self->recognized_list)) {
    list_for_each_safe(pos, q, &self->recognized_list) {
      struct recognized_info *info =
        list_entry (pos, struct recognized_info, list);
      if (info->uid == uid) {
        found = TRUE;
        GstClockTimeDiff diff = GST_CLOCK_DIFF(info->timestamp, now);
        if (GST_TIME_AS_SECONDS(diff) >= self->recog_trigger_timeout) {
          info->timestamp = now;
          trigger = TRUE;
        }
      }
    }
  }

  if (!found) {
    struct recognized_info *info = g_new(struct recognized_info, 1);
    info->uid = uid;
    info->name = g_strdup(name);
    info->timestamp = now;
    list_init (&info->list);
    list_add_tail(&self->recognized_list, &info->list);
    trigger = TRUE;
  }

  if (trigger) {
    g_signal_emit(self, gst_amlnn_signals[SIGNAL_FACE_RECOGNIZED], 0, uid,
                  name);
  }
}

static void cleanup_recognized_list(GstAmlNN *self) {
  struct listnode *pos, *q;
  if (!list_empty(&self->recognized_list)) {
    list_for_each_safe(pos, q, &self->recognized_list) {
      struct recognized_info *info =
        list_entry (pos, struct recognized_info, list);
      list_remove (pos);
      g_free(info->name);
    }
  }
}

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

  nn->imgproc = NULL;
  nn->prebuf.width = NN_INPUT_BUF_W;
  nn->prebuf.height = NN_INPUT_BUF_H;
  nn->prebuf.size = NN_INPUT_BUF_SIZE;
  nn->prebuf.format = NN_INPUT_BUF_FORMAT;

  g_cond_init(&nn->_cond);
  g_mutex_init(&nn->_mutex);
  nn->_ready = FALSE;

  list_init(&nn->recognized_list);
  nn->recog_trigger_timeout = DEFAULT_PROP_RECOG_TRIGGER_TIMEOUT;

  // set debug log level
  det_set_log_config(DET_DEBUG_LEVEL_WARN,DET_LOG_TERMINAL);
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

  if (m->outmem) {
    gst_memory_unref (m->outmem);
    m->outmem = NULL;
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
  if (data == NULL || data->self == NULL || data->self->_running == FALSE ||
      data->u.model.minfo == NULL) {
    return G_SOURCE_REMOVE;
  }

  GstAmlNN *self = data->self;
  ModelInfo *minfo = data->u.model.minfo;

  g_mutex_lock(&self->_mutex);
  // close detect model for the next reinitialization
  close_model(minfo);
  minfo->model = data->u.model.new_model;
  if (data->u.model.new_model == DET_BUTT) {
    // notify the overlay to clear info
    push_result (&self->element, NULL);
  }
  g_mutex_unlock(&self->_mutex);

  g_free(data);
  return G_SOURCE_REMOVE;
}

static gboolean idle_close_db(struct idle_task_data *data) {
  if (data == NULL || data->self == NULL || data->self->_running == FALSE ||
      data->u.db.file == NULL) {
    return G_SOURCE_REMOVE;
  }

  GstAmlNN *self = data->self;

  g_mutex_lock(&self->_mutex);
  close_db(&self->db_param);
  g_free(self->db_param.file);
  self->db_param.file = data->u.db.file;
  g_mutex_unlock(&self->_mutex);

  g_free(data);
  return G_SOURCE_REMOVE;
}

static gboolean idle_request_capface(struct idle_task_data *data) {
  if (data == NULL || data->self == NULL || data->self->_running == FALSE) {
    return G_SOURCE_REMOVE;
  }

  GstAmlNN *self = data->self;

  g_mutex_lock(&self->_mutex);
  self->db_param.bstore_face = TRUE;
  g_mutex_unlock(&self->_mutex);

  g_free(data);
  return G_SOURCE_REMOVE;
}

static gboolean idle_request_capface_from_image(struct idle_task_data *data) {
  if (data == NULL || data->self == NULL || data->self->_running == FALSE ||
      data->u.img.file == NULL) {
    return G_SOURCE_REMOVE;
  }

  GstAmlNN *self = data->self;

  g_mutex_lock(&self->_mutex);
  if (self->custimg) g_free(self->custimg);
  self->custimg = data->u.img.file;
  g_mutex_unlock(&self->_mutex);

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

  self->imgproc = imgproc_init();
  if (self->imgproc == NULL) {
    GST_ERROR_OBJECT(self, "failed to initialize imgproc");
    return FALSE;
  }

  self->_running = TRUE;
  self->_thread = g_thread_new("nn process", amlnn_process, self);
  return TRUE;
}

static gboolean gst_aml_nn_close(GstBaseTransform *base) {
  GstAmlNN *self = GST_AMLNN(base);

  GST_DEBUG_OBJECT(self, "closing, waiting for lock");
  self->_running = FALSE;
  g_mutex_lock(&self->_mutex);
  self->_ready = TRUE;
  g_cond_signal(&self->_cond);
  g_mutex_unlock(&self->_mutex);
  g_thread_join(self->_thread);

  self->_thread = NULL;

  if (self->custimg) {
    g_free(self->custimg);
    self->custimg = NULL;
  }
  if (self->imgproc) {
    imgproc_deinit (self->imgproc);
    self->imgproc = NULL;
  }

  cleanup_recognized_list(self);
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
    GST_ERROR_OBJECT(base, "caps are invalid");
    return FALSE;
  }
  self->info = info;
  self->is_info_set = TRUE;

  return TRUE;
}

/* GstBaseTransform vmethod implementations */
static gboolean gst_aml_nn_sink_event(GstBaseTransform *base, GstEvent *event) {
  GstAmlNN *self = GST_AMLNN(base);

  GST_INFO_OBJECT(base, "gst_aml_nn_sink_event");

  switch (GST_EVENT_TYPE(event)) {
  case GST_EVENT_CUSTOM_DOWNSTREAM_OOB: {
    const GstStructure *st = gst_event_get_structure(event);

    GST_INFO_OBJECT(base, "gst_aml_nn_sink_event, GST_EVENT_CUSTOM_DOWNSTREAM_OOB");

    if (gst_structure_has_name(st, "do-image-facecap")) {

      GST_INFO_OBJECT(base, "gst_aml_nn_sink_event, do-image-facecap");

      const GValue *value = gst_structure_get_value(st, "image-path");
      struct idle_task_data *data = g_new(struct idle_task_data, 1);
      data->self = self;
      data->u.img.file = g_value_dup_string (value);
      g_idle_add((GSourceFunc)idle_request_capface_from_image, data);
    }
    if (gst_structure_has_name(st, "do-facecap")) {

      GST_INFO_OBJECT(base, "gst_aml_nn_sink_event, do-facecap");

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
                        struct nn_result_buffer *resbuf) {
  GstMapInfo info;

  GST_INFO_OBJECT(base, "push_result, resbuf=%p", resbuf);

  if (resbuf == NULL || resbuf->amount <= 0) {

  GST_INFO_OBJECT(base, "push_result, nn-result-clear");
    GstStructure *st = gst_structure_new("nn-result-clear", NULL, NULL);
    GstEvent *event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, st);
    gst_element_send_event(&base->element, event);
    return;
  }

  GST_INFO_OBJECT(base, "push_result, resbuf->amount=%d", resbuf->amount);

  gint st_size = sizeof(struct nn_result_buffer);
  gint res_size = resbuf->amount * sizeof(struct nn_result);

  GstBuffer *gstbuf = gst_buffer_new_allocate(NULL, st_size + res_size, NULL);
  if (gst_buffer_map(gstbuf, &info, GST_MAP_WRITE)) {
    memcpy(info.data, resbuf, st_size);
    memcpy(info.data + st_size, resbuf->results, res_size);
    gst_buffer_unmap(gstbuf, &info);

    GstStructure *st = gst_structure_new("nn-result", "result-buffer",
                                         GST_TYPE_BUFFER, gstbuf, NULL);

    GstEvent *event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, st);

    GST_INFO_OBJECT(base, "push_result, gst_element_send_event");

    gst_element_send_event(&base->element, event);
  }
  gst_buffer_unref(gstbuf);
}

static GstMemory *process_custom_image(GstAmlNN *self) {
  int width, height, stride;
  GstMemory *input_memory = NULL;
  GstMemory *output_memory = NULL;
  GstMemory *ret_memory = NULL;
  GstMapInfo minfo;

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
  if (width != self->prebuf.width || height != self->prebuf.height) {
    // the input jpeg is not fit the current input buffer
    // use ge2d to resize
    struct imgproc_buf inbuf, outbuf;

    inbuf.fd = gst_dmabuf_memory_get_fd(input_memory);
    inbuf.is_ionbuf = TRUE;
    if (inbuf.fd < 0) {
      GST_ERROR_OBJECT(self, "failed to obtain the input memory fd");
      goto fail_exit;
    }

    output_memory =
          gst_allocator_alloc(self->dmabuf_alloc, self->prebuf.size, NULL);
    if (output_memory == NULL) {
      GST_ERROR_OBJECT(self, "failed to allocate new dma buffer");
      goto fail_exit;
    }

    outbuf.fd = gst_dmabuf_memory_get_fd(output_memory);
    outbuf.is_ionbuf = TRUE;

    struct imgproc_pos inpos = {0, 0, width, height, width, height};
    struct imgproc_pos outposition = {
        0,
        0,
        width > self->prebuf.width ? self->prebuf.width : width,
        height > self->prebuf.height ? self->prebuf.height : height,
        self->prebuf.width,
        self->prebuf.height};

    if (!imgproc_crop(self->imgproc,
                      inbuf, inpos, GST_VIDEO_FORMAT_RGB,
                      outbuf, outposition, self->prebuf.format)) {
      GST_ERROR_OBJECT(self, "failed to process buffer");
      goto fail_exit;
    }
    ret_memory = gst_memory_ref (output_memory);
  } else {
    ret_memory = gst_memory_ref (input_memory);
  }

fail_exit:
  if (input_memory) {
    gst_memory_unref (input_memory);
  }
  if (output_memory) {
    gst_memory_unref (output_memory);
  }
  return ret_memory;
}




static gboolean detection_process(GstAmlNN *self, struct imgproc_buf inbuf,
                                  struct nn_result_buffer *resbuf) {
  gboolean ret = TRUE;

  GST_INFO_OBJECT(self, "face detection process begin");

  if (!self->face_det.initialized) {
    GST_INFO("open_model, model=%d", self->face_det.model);
    open_model(&self->face_det);
    self->face_det.param.param.det_param.detect_num = self->max_detect_num;
    det_set_param(self->face_det.model, self->face_det.param);
  }

  if (!self->face_det.initialized) {
    GST_ERROR_OBJECT(self, "face detection model not initialized");
    return FALSE;
  }

  struct timeval st;
  struct timeval mid;
  struct timeval ed;
  double time_total;
  gettimeofday(&st, NULL);

  struct imgproc_buf outbuf;

  GST_INFO_OBJECT(self, "detection_process dmabuf_alloc=%p, stride=%d, height=%d, outmem=%p",
    self->dmabuf_alloc, self->face_det.stride, self->face_det.height, self->face_det.outmem);

  if (self->face_det.outmem == NULL) {
    GST_INFO_OBJECT(self, "detection_process gst_allocator_alloc");

    self->face_det.outmem = gst_allocator_alloc(
        self->dmabuf_alloc,
        self->face_det.stride * self->face_det.height, NULL);
    if (self->face_det.outmem == NULL) {
      GST_ERROR_OBJECT(self, "failed to allocate the output dma buffer, %p, %d",
        self->dmabuf_alloc, self->face_det.stride * self->face_det.height);
      return FALSE;
    }
  }

  outbuf.fd = gst_dmabuf_memory_get_fd(self->face_det.outmem);

  GST_INFO_OBJECT(self, "detection_process outmem=%p, fd=%d",
    self->face_det.outmem, outbuf.fd);


  GST_INFO_OBJECT(self, "detection_process imgproc =%p, inbuf.fd =%d, outbuf.fd=%d",
    self->imgproc, inbuf.fd, outbuf.fd);
  GST_INFO_OBJECT(self, "detection_process prebuf.width =%d, prebuf.height =%d, face_det.width =%d, face_det.height =%d",
    self->prebuf.width,
    self->prebuf.height,
    self->face_det.width,
    self->face_det.height);

  // Convert source image
  // Color format -> RGB
  // width -> 480; height -> 480
  outbuf.is_ionbuf = TRUE;
  struct imgproc_pos inpos = {0,
                              0,
                              self->prebuf.width,
                              self->prebuf.height,
                              self->prebuf.width,
                              self->prebuf.height};
  struct imgproc_pos outposition = {0,
                               0,
                               self->face_det.width,
                               self->face_det.height,
                               self->face_det.width,
                               self->face_det.height};

  if (!imgproc_crop(self->imgproc, inbuf, inpos,
                    self->prebuf.format, outbuf, outposition,
                    GST_VIDEO_FORMAT_RGB)) {
    GST_ERROR_OBJECT(self, "failed to resize the input buffer");
    return FALSE;
  }

  GST_INFO_OBJECT(self, "detection_process befor map, outmem=%p", self->face_det.outmem);

  gettimeofday(&mid, NULL);
  time_total = (mid.tv_sec - st.tv_sec)*1000000.0 + (mid.tv_usec - st.tv_usec);
  GST_INFO_OBJECT(self, "imgproc_crop, time=%lf uS", time_total);

  GstMapInfo minfo;
  if (!gst_memory_map(self->face_det.outmem, &minfo, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT(self, "failed to map output detection buffer");
    return FALSE;
  }

  GST_INFO_OBJECT(self, "detection_process rowbytes=%d, stride=%d",
    self->face_det.rowbytes, self->face_det.stride);

  if (self->face_det.rowbytes != self->face_det.stride) {
    gint rowbytes = self->face_det.rowbytes;
    gint stride = self->face_det.stride;
    for (int i = 0; i < self->face_det.height; i++) {
      memcpy(&minfo.data[rowbytes * i], &minfo.data[stride * i], rowbytes);
    }
  }

  GST_INFO_OBJECT(self, "detection_process minfo.data=%p, width=%d, height=%d, channel=%d",
    minfo.data, self->face_det.width, self->face_det.height, self->face_det.channel);

  input_image_t im;
  im.data = minfo.data;
  im.pixel_format = PIX_FMT_RGB888;
  im.width = self->face_det.width;
  im.height = self->face_det.height;
  im.channel = self->face_det.channel;
  det_status_t rc = det_set_input(im, self->face_det.model);
  gst_memory_unmap(self->face_det.outmem, &minfo);

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

  resbuf->amount = res.result.det_result.detect_num;
  resbuf->results = NULL;
  if (res.result.det_result.detect_num > 0) {
    resbuf->results = g_new(struct nn_result, res.result.det_result.detect_num);
    for (gint i = 0; i < res.result.det_result.detect_num; i++) {
      resbuf->results[i].pos.left = res.result.det_result.point[i].point.rectPoint.left/im.width;
      resbuf->results[i].pos.top = res.result.det_result.point[i].point.rectPoint.top/im.height;
      resbuf->results[i].pos.right = res.result.det_result.point[i].point.rectPoint.right/im.width;
      resbuf->results[i].pos.bottom = res.result.det_result.point[i].point.rectPoint.bottom/im.height;
      for (gint j = 0; j < 5; j++) {
        resbuf->results[i].fpos[j].x = res.result.det_result.point[i].tpts.floatX[j]/im.width;
        resbuf->results[i].fpos[j].y = res.result.det_result.point[i].tpts.floatY[j]/im.height;
      }
      resbuf->results[i].info[0] = '\0';
    }
  }

  g_free(res.result.det_result.point);
  g_free(res.result.det_result.result_name);



  gettimeofday(&ed, NULL);
  time_total = (ed.tv_sec - mid.tv_sec)*1000000.0 + (ed.tv_usec - mid.tv_usec);

  GST_INFO_OBJECT(self, "face detect, time=%lf uS", time_total);


#else

  int x_rand;
  int y_rand;
  int w_rand;

  DetectResult res;
  res.result.det_result.detect_num = 200;
  res.result.det_result.point = g_new(det_position_float_t, self->face_det.param.param.det_param.detect_num);
  res.result.det_result.result_name = g_new(det_classify_result_t, self->face_det.param.param.det_param.detect_num);
  det_get_result(&res, self->face_det.model);
  GST_INFO_OBJECT(self, "detection result got, facenum: %d, maxfacenum: %d",
    res.result.det_result.detect_num, self->face_det.param.param.det_param.detect_num);

  // fleet temp
  resbuf->amount = res.result.det_result.detect_num;
  resbuf->results = g_new(struct nn_result, resbuf->amount);

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

  GST_INFO_OBJECT(self, "face detection process finished");
  return ret;
}

static gboolean recognition_process(GstAmlNN *self, struct imgproc_buf inbuf,
                                    struct nn_result_buffer *resbuf) {
  gboolean ret = TRUE;

  if (self->face_recog.model == DET_BUTT)
    return ret;

  if (resbuf == NULL || resbuf->amount == 0) {
    return ret;
  }

  GST_DEBUG_OBJECT(self, "face recognition process begin");

  if (!self->face_recog.initialized) {
    open_model(&self->face_recog);
  }

  if (!self->face_recog.initialized) {
    GST_ERROR_OBJECT(self, "face recognition model not initialized");
    return FALSE;
  }

  if (self->db_param.handle == NULL) {
    db_set_scale(self->face_recog.param.param.reg_param.scale);
    open_db(&self->db_param);
  }

  if (self->db_param.handle == NULL) {
    GST_DEBUG_OBJECT(self, "database not set");
    return FALSE;
  }

  struct imgproc_buf outbuf;

  if (self->face_recog.outmem == NULL) {
    self->face_recog.outmem = gst_allocator_alloc(
        self->dmabuf_alloc,
        self->face_recog.stride * self->face_recog.height, NULL);
    if (self->face_recog.outmem == NULL) {
      GST_ERROR_OBJECT(self, "failed to allocate the output dma buffer");
      return FALSE;
    }
  }

  outbuf.fd = gst_dmabuf_memory_get_fd(self->face_recog.outmem);
  outbuf.is_ionbuf = TRUE;

  GstMapInfo minfo;
  if (!gst_memory_map(self->face_recog.outmem, &minfo, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT(self, "failed to map output recognition buffer");
    return FALSE;
  }

  struct imgproc_pos inpos = {0,
                              0,
                              self->prebuf.width,
                              self->prebuf.height,
                              self->prebuf.width,
                              self->prebuf.height};
  struct imgproc_pos outposition = {0,
                               0,
                               self->face_recog.width,
                               self->face_recog.height,
                               self->face_recog.width,
                               self->face_recog.height};

  for (gint i = 0; i < resbuf->amount; i++) {
    struct {
      gint x0;
      gint y0;
      gint x1;
      gint y1;
      gint w;
      gint h;
    } detect_rect, recog_rect;

    detect_rect.x0 = (int)(resbuf->results[i].pos.left * self->prebuf.width);
    detect_rect.y0 = (int)(resbuf->results[i].pos.top * self->prebuf.height);
    detect_rect.x1 = (int)(resbuf->results[i].pos.right * self->prebuf.width);
    detect_rect.y1 = (int)(resbuf->results[i].pos.bottom * self->prebuf.height);
    detect_rect.w = detect_rect.x1 - detect_rect.x0;
    detect_rect.h = detect_rect.y1 - detect_rect.y0;

    float recog_rel_x, recog_rel_y;
    // adjust the detection rect into to recog square
    if (detect_rect.w > detect_rect.h) {
      recog_rect.w = self->face_recog.width;
      recog_rect.h = (detect_rect.h * self->face_recog.width) / detect_rect.w;
      recog_rect.x0 = 0;
      recog_rect.y0 = (self->face_recog.height - recog_rect.h) / 2;
    } else if(detect_rect.w < detect_rect.h){
      recog_rect.h = self->face_recog.height;
      recog_rect.w = (detect_rect.w * self->face_recog.height) / detect_rect.h;
      recog_rect.x0 = (self->face_recog.width - recog_rect.w) / 2;
      recog_rect.y0 = 0;
    } else {
      recog_rect.h = self->face_recog.height;
      recog_rect.w = self->face_recog.width;
      recog_rect.x0 = 0;
      recog_rect.y0 = 0;
    }

    recog_rel_x = (float)recog_rect.x0 / (float)self->face_recog.width;
    recog_rel_y = (float)recog_rect.y0 / (float)self->face_recog.height;

    outposition.x = 0; outposition.y = 0;
    outposition.w = self->face_recog.width; outposition.h = self->face_recog.height;
    imgproc_fillrect(self->imgproc, GST_VIDEO_FORMAT_RGB, outbuf, outposition,
                     0xffffffff);

    inpos.x = detect_rect.x0; inpos.y = detect_rect.y0;
    inpos.w = detect_rect.w; inpos.h = detect_rect.h;
    outposition.x = recog_rect.x0; outposition.y = recog_rect.y0;
    outposition.w = recog_rect.w; outposition.h = recog_rect.h;

    if (!imgproc_crop(self->imgproc, inbuf, inpos,
                      self->prebuf.format, outbuf, outposition,
                      GST_VIDEO_FORMAT_RGB)) {
      GST_ERROR_OBJECT(self, "resize input buffer failed");
      ret = FALSE;
      goto recognition_process_exit;
    }

   if (self->face_recog.rowbytes != self->face_recog.stride) {
      gint rowbytes = self->face_recog.rowbytes;
      gint stride = self->face_recog.stride;
      for (int i=0; i<self->face_recog.height; i++) {
        memcpy(&minfo.data[rowbytes * i], &minfo.data[stride * i], rowbytes);
      }
    }

    // due to the input image would be modified inside nn recoginition module,
    // original image data should be saved for database recording purpose
    gint img_length = self->face_recog.rowbytes * self->face_recog.height;
    gchar *im_data = g_new(gchar, img_length);
    memcpy (im_data, minfo.data,  img_length);

    input_image_t im;
    im.data = im_data;
    im.pixel_format = PIX_FMT_RGB888;
    im.width = self->face_recog.width;
    im.height = self->face_recog.height;
    im.channel = self->face_recog.channel;
    for (gint fcnt = 0; fcnt < 5; fcnt++) {
      if (resbuf->results[i].fpos[fcnt].x >= resbuf->results[i].pos.left &&
          resbuf->results[i].fpos[fcnt].y >= resbuf->results[i].pos.top) {
        im.inPoint[fcnt].x =
            (resbuf->results[i].fpos[fcnt].x - resbuf->results[i].pos.left) /
                (resbuf->results[i].pos.right - resbuf->results[i].pos.left) +
            recog_rel_x;
        im.inPoint[fcnt].y =
            (resbuf->results[i].fpos[fcnt].y - resbuf->results[i].pos.top) /
                (resbuf->results[i].pos.bottom - resbuf->results[i].pos.top) +
            recog_rel_y;
      } else {
        im.inPoint[fcnt].x = 0.0;
        im.inPoint[fcnt].y = 0.0;
      }
    }

    det_status_t rc = det_set_input(im, self->face_recog.model);
    g_free(im_data);

    if (rc != DET_STATUS_OK) {
      GST_ERROR_OBJECT(self, "failed to set input to recognition model");
      ret = FALSE;
      goto recognition_process_exit;
    }

    DetectResult fn_res;
    GST_DEBUG_OBJECT(self, "waiting for recognition result");
    rc = det_get_result(&fn_res, self->face_recog.model);
    GST_DEBUG_OBJECT(self, "recognition result got");
    if (rc != DET_STATUS_OK) {
      GST_ERROR_OBJECT(self, "failed to get recognition result");
      continue;
    }

    int result_length = self->face_recog.param.param.reg_param.length;
    GST_DEBUG_OBJECT (self, "recoginition vector number = %d", result_length);

    int uid =
        db_search_result(self->db_param.handle, fn_res.result.reg_result.uint8,
                         result_length, self->db_param.bstore_face ? minfo.data : NULL,
                         self->face_recog.width, self->face_recog.height,
                         self->db_param.format, resbuf->results[i].info, FACE_INFO_BUFSIZE);

    if (uid < 0) {
      resbuf->results[i].info[0] = '\0';
    } else {
      trigger_recognized(self, uid, resbuf->results[i].info);
    }
  }

recognition_process_exit:

  gst_memory_unmap(self->face_recog.outmem, &minfo);
  self->db_param.bstore_face = FALSE;

  GST_DEBUG_OBJECT(self, "face recognition process finished");
  return ret;
}

static gpointer amlnn_process(void *data) {
  struct nn_result_buffer resbuf;
  GstAmlNN *self = (GstAmlNN *)data;
  GstMemory *input_memory = NULL;
  gboolean is_normal_process = TRUE;
  struct imgproc_buf inbuf;

  GST_INFO_OBJECT(self, "amlnn_process begin, _running=%d, _ready=%d", self->_running, self->_ready);

  while (self->_running) {
    g_mutex_lock(&self->_mutex);
    while (!self->_ready) {
      g_cond_wait(&self->_cond, &self->_mutex);
    }

    GST_INFO_OBJECT(self, "amlnn_process wait _cond done, model=%d", self->face_det.model);

    if (!self->_running) {
      goto loop_continue;
    }

    if (self->face_det.model == DET_BUTT) {
      // face detection not enabled,
      // ignore the request and exit
      GST_DEBUG_OBJECT(self, "face detection model disabled");
      // make sure the input memory would be released
      input_memory = self->nn_imem;
      goto loop_continue;
    }

    if (self->dmabuf_alloc == NULL) {
      self->dmabuf_alloc = gst_amlion_allocator_obtain();
    }

    GST_INFO_OBJECT(self, "amlnn_process loop, custimg=%s", self->custimg);
    GST_INFO_OBJECT(self, "amlnn_process loop, nn_imem=%p", self->nn_imem);

    if (self->custimg) {
      input_memory = process_custom_image(self);
      g_free(self->custimg);
      self->custimg = NULL;
      if (input_memory == NULL) goto loop_continue;
      self->db_param.bstore_face = TRUE;
      is_normal_process = FALSE;
    } else if (self->nn_imem) {
      input_memory = self->nn_imem;
      is_normal_process = TRUE;
    }

    GST_INFO_OBJECT(self, "amlnn_process loop, input_memory=%p", input_memory);
    GST_INFO_OBJECT(self, "amlnn_process loop, is_normal_process=%d", is_normal_process);

    if (input_memory == NULL) {
      GST_ERROR_OBJECT(self, "input buffer not valid");
      goto loop_continue;
    }

    inbuf.fd = gst_dmabuf_memory_get_fd(input_memory);
    inbuf.is_ionbuf = gst_is_amlionbuf_memory(input_memory);

    detection_process(self, inbuf, &resbuf);

    // fleet temp
    //recognition_process(self, inbuf, &resbuf);

    if (is_normal_process && self->_running) {
      push_result(&self->element, &resbuf);
    }

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

    self->nn_imem = NULL;

    // continue process buffer
    if (self->_running) {
      gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(self), FALSE);
    }
    self->_ready = FALSE;
    g_mutex_unlock(&self->_mutex);
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

  GST_INFO_OBJECT(base, "gst_aml_nn_transform_ip begin");

  if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(outbuf)))
    gst_object_sync_values(GST_OBJECT(self), GST_BUFFER_TIMESTAMP(outbuf));

  if (!self->is_info_set) {
    GST_ELEMENT_ERROR(base, CORE, NEGOTIATION, (NULL), ("unknown format"));
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (self->face_det.model == DET_BUTT) {
    // face detection not enabled
    return GST_FLOW_OK;
  }

  GstMemory *input_memory = gst_buffer_get_memory(outbuf, 0);
  gboolean is_dmabuf = gst_is_dmabuf_memory(input_memory);
  struct imgproc_buf inbuf, nn_ibuf;

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

    GST_INFO_OBJECT(base, "allocate memory, info.size %ld", self->info.size);

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

  inbuf.fd = gst_dmabuf_memory_get_fd(input_memory);
  inbuf.is_ionbuf = gst_is_amlionbuf_memory(input_memory);

  GST_INFO_OBJECT(base, "allocate memory, prebuf.size %d", self->prebuf.size);

  GstMemory *nn_imem = gst_allocator_alloc(self->dmabuf_alloc, self->prebuf.size, NULL);
  if (nn_imem == NULL) {
    GST_ERROR_OBJECT(self, "failed to allocate new dma buffer");
    goto transform_end;
  }

  GST_INFO_OBJECT(base, "gst_aml_nn_transform_ip gst_allocator_alloc nn_imem done");
  nn_ibuf.fd = gst_dmabuf_memory_get_fd(nn_imem);
  GST_INFO_OBJECT(base, "gst_aml_nn_transform_ip gst_dmabuf_memory_get_fd nn_imem done");
  nn_ibuf.is_ionbuf = gst_is_amlionbuf_memory(nn_imem);

  GST_INFO_OBJECT(base, "gst_aml_nn_transform_ip gst_is_amlionbuf_memory nn_imem done");

  struct imgproc_pos inpos = {0,
                              0,
                              self->info.width,
                              self->info.height,
                              self->info.width,
                              self->info.height};
  struct imgproc_pos outposition = {0,
                               0,
                               self->prebuf.width,
                               self->prebuf.height,
                               self->prebuf.width,
                               self->prebuf.height};

  if (!imgproc_crop(self->imgproc, inbuf, inpos,
                    GST_VIDEO_INFO_FORMAT(&self->info), nn_ibuf, outposition,
                    self->prebuf.format)) {
    gst_memory_unref(nn_imem);
    GST_ERROR_OBJECT(self, "failed to process buffer");
    goto transform_end;
  }

  gettimeofday(&ed, NULL);
  time_total = (ed.tv_sec - st.tv_sec)*1000000.0 + (ed.tv_usec - st.tv_usec);
  GST_INFO_OBJECT(base, "gst_aml_nn_transform_ip imgproc_crop done, time=%lf uS", time_total);

  if (g_mutex_trylock(&self->_mutex)) {
    // skip the following transform
    gst_base_transform_set_passthrough(base, TRUE);

    self->nn_imem = nn_imem;
    self->_ready = TRUE;
    g_cond_signal(&self->_cond);
    g_mutex_unlock(&self->_mutex);
  }

  GST_INFO_OBJECT(base, "gst_aml_nn_transform_ip end");

  ret = GST_FLOW_OK;

transform_end:
  if (input_memory != NULL) {
    gst_memory_unref(input_memory);
  }
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
