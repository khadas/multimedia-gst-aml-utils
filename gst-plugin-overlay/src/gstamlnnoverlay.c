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
 * SECTION:element-amlnnoverlay
 *
 * FIXME:Describe amlnnoverlay here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! amlnnoverlay ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/allocators/gstamlionallocator.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "gfx_2d.h"
#include "gstamlnnoverlay.h"



#define PRINT_FPS

#ifdef PRINT_FPS
#define PRINT_FPS_INTERVAL_S 120

static int64_t get_current_time_msec(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

#endif



GST_DEBUG_CATEGORY_STATIC(gst_aml_nn_overlay_debug);
#define GST_CAT_DEFAULT gst_aml_nn_overlay_debug

#define DETECT_RESULT_KEEP_MS 5000 // in case of event lost

#define DEFAULT_PROP_ENABLED TRUE
#define DEFAULT_PROP_FONTFILE "/usr/share/directfb-1.7.7/decker.ttf"
#define DEFAULT_PROP_FONTSIZE 24


#define DEFAULT_PROP_FONTCOLOR 0x00ffffff

#define DEFAULT_PROP_RECTCOLOR 0xff0000ff
#define TEMP_SURFACE_COLOR_FORMAT GST_VIDEO_FORMAT_RGBA

#define DEFAULT_PROP_RECT_THICKNESS 6



// RGBA
enum {
  PROP_0,
  PROP_ENABLED,
  PROP_FONTCOLOR,
  PROP_FONTFILE,
  PROP_FONTSIZE,
  PROP_RECTCOLOR,
};

#define gst_aml_nn_overlay_parent_class parent_class
G_DEFINE_TYPE(GstAmlNNOverlay, gst_aml_nn_overlay, GST_TYPE_AMLOVERLAY);

static void gst_aml_nn_overlay_set_property(GObject *object, guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void gst_aml_nn_overlay_get_property(GObject *object, guint prop_id,
                                            GValue *value, GParamSpec *pspec);

static void gst_aml_nn_overlay_finalize(GObject *object);

static gboolean gst_aml_nn_overlay_start(GstBaseTransform *trans);
static gboolean gst_aml_nn_overlay_stop(GstBaseTransform *trans);

static GstFlowReturn gst_aml_nn_overlay_transform_ip(GstBaseTransform *trans,
                                                     GstBuffer *outbuf);

static gboolean gst_aml_nn_overlay_event(GstBaseTransform *trans,
                                         GstEvent *event);
static void nn_release_result(NNResultBuffer *resbuf);

static gpointer overlay_process(void *data);

/* GObject vmethod implementations */

/* initialize the amloverlay's class */
static void gst_aml_nn_overlay_class_init(GstAmlNNOverlayClass *klass) {
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;

  GST_DEBUG_CATEGORY_INIT(gst_aml_nn_overlay_debug, "amlnnoverlay", 0,
                          "amlogic nn overlay");

  gobject_class->set_property = gst_aml_nn_overlay_set_property;
  gobject_class->get_property = gst_aml_nn_overlay_get_property;
  gobject_class->finalize = gst_aml_nn_overlay_finalize;

  g_object_class_install_property(
      gobject_class, PROP_ENABLED,
      g_param_spec_boolean("enabled", "Enabled", "enable/disable nn overlay",
                           DEFAULT_PROP_ENABLED,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      G_OBJECT_CLASS(klass), PROP_FONTFILE,
      g_param_spec_string(
          "font-file", "Font-File", "Truetype font file for display NN info",
          DEFAULT_PROP_FONTFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      G_OBJECT_CLASS(klass), PROP_FONTSIZE,
      g_param_spec_int("font-size", "Font-Size", "Font size for NN info", 8,
                       256, DEFAULT_PROP_FONTSIZE,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      G_OBJECT_CLASS(klass), PROP_FONTCOLOR,
      g_param_spec_uint(
          "font-color", "Font-Color", "Color to use for NN info (RGBA).", 0,
          G_MAXUINT32, DEFAULT_PROP_FONTCOLOR,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      G_OBJECT_CLASS(klass), PROP_RECTCOLOR,
      g_param_spec_uint(
          "rect-color", "Rect-Color", "Color to use for NN rectangel (RGBA).",
          0, G_MAXUINT32, DEFAULT_PROP_RECTCOLOR,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_details_simple(
      gstelement_class, "Amlogic NN Overlay", "Filter/Editor/Video",
      "Draw NN info on each frame", "Jemy Zhang <jun.zhang@amlogic.com>");

  GST_BASE_TRANSFORM_CLASS(klass)->start =
      GST_DEBUG_FUNCPTR(gst_aml_nn_overlay_start);

  GST_BASE_TRANSFORM_CLASS(klass)->stop =
      GST_DEBUG_FUNCPTR(gst_aml_nn_overlay_stop);

  GST_BASE_TRANSFORM_CLASS(klass)->transform_ip =
      GST_DEBUG_FUNCPTR(gst_aml_nn_overlay_transform_ip);

  GST_BASE_TRANSFORM_CLASS(klass)->sink_event =
      GST_DEBUG_FUNCPTR(gst_aml_nn_overlay_event);
}

/* initialize the new element
 * initialize instance structure
 */
static void gst_aml_nn_overlay_init(GstAmlNNOverlay *self) {
  GstAmlOverlay *base = GST_AMLOVERLAY_CAST(self);

  self->nn.enabled = DEFAULT_PROP_ENABLED;
  self->nn.rectcolor = DEFAULT_PROP_RECTCOLOR;
  self->nn.font.ttf = g_strdup(DEFAULT_PROP_FONTFILE);
  self->nn.font.fgcolor = DEFAULT_PROP_FONTCOLOR;
  self->nn.font.size = DEFAULT_PROP_FONTSIZE;

  gst_aml_overlay_init_surface_struct(&self->nn.hfont, &base->graphic.surface_lock);

  self->nn.result.srcid = 0;
  g_mutex_init(&self->nn.result.lock);

  g_cond_init(&self->m_cond);
  g_mutex_init(&self->m_mutex);
  self->m_ready = FALSE;
  self->m_running = TRUE;

  base->graphic.render_idx = -1;
  base->graphic.cur_display_idx = -1;
  base->graphic.next_display_idx = -1;
  base->process = overlay_process;
}

static void gst_aml_nn_overlay_finalize(GObject *object) {
  GstAmlNNOverlay *self = GST_AMLNNOVERLAY(object);
  GstAmlOverlay *base = GST_AMLOVERLAY_CAST(self);

  self->m_running = FALSE;

  g_mutex_lock(&self->m_mutex);
  self->m_ready = TRUE;
  g_cond_signal(&self->m_cond);
  g_mutex_unlock(&self->m_mutex);

  if (NULL != base->m_thread) {
    g_thread_join(base->m_thread);
    base->m_thread = NULL;
  }

  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static gboolean gst_aml_nn_overlay_start(GstBaseTransform *trans) {
  GstAmlNNOverlay *self = GST_AMLNNOVERLAY(trans);
  GST_INFO_OBJECT(self, "Enter");
  return GST_BASE_TRANSFORM_CLASS(parent_class)->start(trans);
}

static gboolean gst_aml_nn_overlay_stop(GstBaseTransform *trans) {
  GstAmlNNOverlay *self = GST_AMLNNOVERLAY(trans);
  GST_INFO_OBJECT(self, "Enter");

  gst_aml_overlay_deinit_surface_struct(&self->nn.hfont);

  AMLOVERLAY_FREE_STRING(self->nn.font.ttf);

  g_mutex_lock(&self->nn.result.lock);

  if (self->nn.result.srcid) {
    g_source_remove(self->nn.result.srcid);
    self->nn.result.srcid = 0;
  }

  if (self->nn.result.buf) {
    nn_release_result(self->nn.result.buf);
    self->nn.result.buf = NULL;
  }

  g_mutex_unlock(&self->nn.result.lock);
  return GST_BASE_TRANSFORM_CLASS(parent_class)->stop(trans);
}

static void gst_aml_nn_overlay_set_property(GObject *object, guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec) {
  GstAmlNNOverlay *self = GST_AMLNNOVERLAY(object);

  switch (prop_id) {
  case PROP_ENABLED:
    self->nn.enabled = g_value_get_boolean(value);
    gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(object),
                                       !self->nn.enabled);
    break;
  case PROP_RECTCOLOR:
    self->nn.rectcolor = g_value_get_uint(value);
    break;
  case PROP_FONTFILE: {
    gchar *f = g_value_dup_string(value);
    if (g_strcmp0(self->nn.font.ttf, f) != 0) {
      g_free(self->nn.font.ttf);
      self->nn.font.ttf = f;
      //gst_aml_overlay_delay_destroy_font(&self->nn.hfont);
    } else {
      g_free(f);
    }
  } break;
  case PROP_FONTCOLOR:
    self->nn.font.fgcolor = g_value_get_uint(value);
    break;
  case PROP_FONTSIZE: {
    guint val = g_value_get_int(value);
    if (val != self->nn.font.size) {
      self->nn.font.size = val;
      //gst_aml_overlay_delay_destroy_font(&self->nn.hfont);
    }
  } break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void gst_aml_nn_overlay_get_property(GObject *object, guint prop_id,
                                            GValue *value, GParamSpec *pspec) {
  GstAmlNNOverlay *self = GST_AMLNNOVERLAY(object);

  switch (prop_id) {
  case PROP_ENABLED:
    g_value_set_boolean(value, self->nn.enabled);
    break;
  case PROP_RECTCOLOR:
    g_value_set_uint(value, self->nn.rectcolor);
    break;
  case PROP_FONTFILE:
    g_value_set_string(value, self->nn.font.ttf);
    break;
  case PROP_FONTCOLOR:
    g_value_set_uint(value, self->nn.font.fgcolor);
    break;
  case PROP_FONTSIZE:
    g_value_set_int(value, self->nn.font.size);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void nn_release_result(NNResultBuffer *resbuf) {
  if (resbuf) {
    if (resbuf->amount > 0) {
      g_free(resbuf->results);
      resbuf->amount = 0;
      resbuf->results = NULL;
    }
    g_free(resbuf);
  }
}


static gboolean timeout_nn_release_result(GstAmlNNOverlay *overlay) {
  g_mutex_lock(&overlay->nn.result.lock);
  overlay->nn.result.srcid = 0;

  nn_release_result(overlay->nn.result.buf);
  overlay->nn.result.buf = NULL;
  g_mutex_unlock(&overlay->nn.result.lock);
  return G_SOURCE_REMOVE;
}

static gboolean gst_aml_nn_overlay_event(GstBaseTransform *trans,
                                         GstEvent *event) {
  GstAmlNNOverlay *self = GST_AMLNNOVERLAY(trans);

  // GST_INFO_OBJECT(self, "Enter");

  switch (GST_EVENT_TYPE(event)) {
  case GST_EVENT_CUSTOM_DOWNSTREAM_OOB: {
    const GstStructure *st = gst_event_get_structure(event);
    if (gst_structure_has_name(st, "nn-result")) {

      GST_INFO_OBJECT(self, "nn-result");

      GstMapInfo info;
      const GValue *val = gst_structure_get_value(st, "result-buffer");
      GstBuffer *buf = gst_value_get_buffer(val);

      NNResultBuffer *resbuf = g_new(NNResultBuffer, 1);
      if (gst_buffer_map(buf, &info, GST_MAP_READ)) {

        memcpy(resbuf, info.data, sizeof(NNResultBuffer));

        gint st_size = sizeof(NNResultBuffer);
        gint res_size = resbuf->amount * sizeof(NNResult);

        resbuf->results = g_new(NNResult, resbuf->amount);
        memcpy(resbuf->results, info.data + st_size, res_size);

        GST_INFO_OBJECT(self, "memcpy done, amount=%d", resbuf->amount);

        gst_buffer_unmap(buf, &info);
      } else {
        g_free(resbuf);
        resbuf = NULL;
      }

      if (resbuf) {
        if (self->nn.result.srcid) {
          g_source_remove(self->nn.result.srcid);
        }
        g_mutex_lock(&self->nn.result.lock);
        if (self->nn.result.buf) {
          nn_release_result(self->nn.result.buf);
        }
        self->nn.result.buf = resbuf;
        self->nn.result.srcid = g_timeout_add(
            DETECT_RESULT_KEEP_MS, (GSourceFunc)timeout_nn_release_result,
            (gpointer)self);

        g_mutex_unlock(&self->nn.result.lock);

        // signal overlay_progress to work
        g_mutex_lock(&self->m_mutex);
        self->m_ready = TRUE;
        g_cond_signal(&self->m_cond);
        g_mutex_unlock(&self->m_mutex);

        GST_INFO_OBJECT(self, "signal done, srcid=%d", self->nn.result.srcid);
      }

      gst_event_unref(event);
      return FALSE;
    } else if (gst_structure_has_name(st, "nn-result-clear")) {

      GST_INFO_OBJECT(self, "nn-result-clear");
      g_mutex_lock(&self->nn.result.lock);
      if (self->nn.result.buf) {
        nn_release_result(self->nn.result.buf);
        self->nn.result.buf = NULL;
      }
      g_mutex_unlock(&self->nn.result.lock);
    }
  } break;
  default:
    break;
  }

  //GST_INFO_OBJECT(self, "Leave");

  return GST_BASE_TRANSFORM_CLASS(parent_class)->sink_event(trans, event);
}


/* this function does the actual processing
 */
static GstFlowReturn gst_aml_nn_overlay_transform_ip(GstBaseTransform *trans,
                                                     GstBuffer *outbuf) {
  GstAmlNNOverlay *self = GST_AMLNNOVERLAY(trans);
  GstAmlOverlay *base = GST_AMLOVERLAY(self);
  GstClockTime ts_buffer;
  ts_buffer = GST_BUFFER_TIMESTAMP(outbuf);

  if (GST_CLOCK_TIME_IS_VALID(ts_buffer))
    gst_object_sync_values(GST_OBJECT(self), GST_BUFFER_TIMESTAMP(outbuf));

  if (!base->is_info_set) {
    GST_ELEMENT_ERROR(self, CORE, NEGOTIATION, (NULL), ("unknown format"));
    return GST_FLOW_NOT_NEGOTIATED;
  }

  GstVideoInfo *info = &base->info;

  if (base->graphic.input.fd < 0) {
    GST_ERROR_OBJECT(self, "failed to obtain the input memory fd");
    return GST_FLOW_ERROR;
  }

  struct timeval st;
  struct timeval ed;
  double time_total;
  gettimeofday(&st, NULL);

  GFX_Buf inBuf;
  inBuf.fd = base->graphic.input.fd;
  inBuf.format = gfx_convert_video_format(GST_VIDEO_INFO_FORMAT(info));
  inBuf.is_ionbuf = gst_is_amlionbuf_memory(base->graphic.input.memory);
  inBuf.size.w = info->width;
  inBuf.size.h = info->height;

  GFX_Buf outBuf;
  outBuf.fd = base->graphic.output.fd;
  outBuf.format = gfx_convert_video_format(TEMP_SURFACE_COLOR_FORMAT);
  outBuf.is_ionbuf = gst_is_amlionbuf_memory(base->graphic.output.memory);
  outBuf.size.w = info->width;
  outBuf.size.h = info->height;

  GFX_Handle handle = base->graphic.handle;

  // get display buffer
  g_mutex_lock(&base->graphic.surface_lock);
  int display_idx = base->graphic.next_display_idx;
  base->graphic.cur_display_idx = display_idx;
  GST_INFO_OBJECT(self, "start display, cur_display_idx=%d next_display_idx=%d render_idx=%d",
      base->graphic.cur_display_idx, base->graphic.next_display_idx, base->graphic.render_idx);
  g_mutex_unlock(&base->graphic.surface_lock);

  if (-1 != display_idx) {
    // overlay display buffer to output buffer
    GFX_Buf topBuf;
    topBuf.fd = base->graphic.render[display_idx].fd;
    topBuf.format = gfx_convert_video_format(TEMP_SURFACE_COLOR_FORMAT);
    topBuf.is_ionbuf = gst_is_amlionbuf_memory(base->graphic.render[display_idx].memory);
    topBuf.size.w = info->width;
    topBuf.size.h = info->height;

    GFX_Rect *pDirtyRect = &base->graphic.dirtyRect[display_idx];

    // if input buffer support Alpha, can blend directly
    if (inBuf.format == GST_VIDEO_FORMAT_RGBA) {
      gfx_blend(handle,
                &inBuf, pDirtyRect,
                &topBuf, pDirtyRect,
                &inBuf, pDirtyRect, 0xFF,
                1);
    } else {
      gfx_blend(handle,
                &inBuf, pDirtyRect,
                &topBuf, pDirtyRect,
                &outBuf, pDirtyRect, 0xFF,
                0);

      gfx_stretchblit(handle,
                &outBuf, pDirtyRect,
                &inBuf, pDirtyRect,
                GFX_AML_ROTATION_0,
                0);

      // sync command to HW, wait the executed complete
      gfx_sync_cmd(handle);
    }

    // update new render buffer
    g_mutex_lock(&base->graphic.surface_lock);
    if (display_idx == base->graphic.next_display_idx) {
      // new display buffer not ready, neednot update render buffer index
      GST_INFO_OBJECT(self, "no new dispay buffer, cur_display_idx=%d next_display_idx=%d render_idx=%d",
          base->graphic.cur_display_idx, base->graphic.next_display_idx, base->graphic.render_idx);
    } else {
      base->graphic.render_idx ++;
      if (RENDER_BUF_CNT == base->graphic.render_idx) {
        base->graphic.render_idx = 0;
      }
      base->graphic.cur_display_idx = -1;
      GST_INFO_OBJECT(self, "update render index, cur_display_idx=%d next_display_idx=%d render_idx=%d",
          base->graphic.cur_display_idx, base->graphic.next_display_idx, base->graphic.render_idx);
    }

    g_mutex_unlock(&base->graphic.surface_lock);

    gettimeofday(&ed, NULL);
    time_total = (ed.tv_sec - st.tv_sec)*1000000.0 + (ed.tv_usec - st.tv_usec);
    GST_INFO_OBJECT(self, "Leave, time=%lf uS", time_total);

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

  return GST_BASE_TRANSFORM_CLASS(parent_class)->transform_ip(trans, outbuf);
}



static gpointer overlay_process(void *data) {
  GstAmlNNOverlay *self = (GstAmlNNOverlay *)data;
  GstAmlOverlay *base = GST_AMLOVERLAY(self);
  GstVideoInfo *info = &base->info;

  GST_INFO_OBJECT(self, "Enter, m_running=%d, m_ready=%d", self->m_running, self->m_ready);

  GFX_Handle handle = base->graphic.handle;

  while (self->m_running) {
    g_mutex_lock(&self->m_mutex);
    while (!self->m_ready) {
      g_cond_wait(&self->m_cond, &self->m_mutex);
    }
    self->m_ready = FALSE;

    GST_INFO_OBJECT(self, "wait m_cond done");

    if (!self->m_running) {
      g_mutex_unlock(&self->m_mutex);
      continue;
    }

    g_mutex_unlock(&self->m_mutex);

    // main process

    g_mutex_lock(&self->nn.result.lock);
    if (!self->nn.enabled) {
      GST_INFO_OBJECT(self, "nn not enabled");
      g_mutex_unlock(&self->nn.result.lock);
      continue;
    }

    if (!self->nn.result.buf) {
      GST_INFO_OBJECT(self, "nn.result.buf is null");
      g_mutex_unlock(&self->nn.result.lock);
      continue;
    }

    // GST_INFO_OBJECT(self, "result.buf=%p, amount=%d, srcid=%d",
    //   self->nn.result.buf, self->nn.result.buf->amount, self->nn.result.srcid);
    int rect_count = self->nn.result.buf->amount;
    NNRenderData *pNNData = g_new0(NNRenderData, rect_count);

    for (gint i = 0; i<rect_count; i++) {
      NNResult *res = &self->nn.result.buf->results[i];
      NNRect *pt = &res->pos;

      pNNData[i].rect.w = (int)((pt->right - pt->left) * info->width);
      pNNData[i].rect.h = (int)((pt->bottom - pt->top) * info->height);

      pNNData[i].rect.x = (int)(pt->left * info->width);
      pNNData[i].rect.y = (int)(pt->top * info->height);

      for (gint j = 0; j<5; j++) {
        pNNData[i].pos[j].x = (int)(res->fpos[j].x * info->width);
        pNNData[i].pos[j].y = (int)(res->fpos[j].y * info->height);
      }
    }

    g_mutex_unlock(&self->nn.result.lock);

    // swith display buffer index
    g_mutex_lock(&base->graphic.surface_lock);
    // if (-1 == base->graphic.render_idx) {
    //   base->graphic.render_idx = 0;
    // }

    if (base->graphic.next_display_idx == base->graphic.render_idx) {
      base->graphic.render_idx ++;
      if (RENDER_BUF_CNT == base->graphic.render_idx) {
        base->graphic.render_idx = 0;
      }
    }

    GST_INFO_OBJECT(self, "surface draw, cur_display_idx=%d next_display_idx=%d render_idx=%d",
        base->graphic.cur_display_idx, base->graphic.next_display_idx, base->graphic.render_idx);

    if (base->graphic.cur_display_idx == base->graphic.render_idx) {
      GST_INFO_OBJECT(self, "buffer full, ignore this render, cur_display_idx=%d next_display_idx=%d render_idx=%d",
        base->graphic.cur_display_idx, base->graphic.next_display_idx, base->graphic.render_idx);
      g_mutex_unlock(&base->graphic.surface_lock);
      goto loop_continue;
    }

    int render_idx = base->graphic.render_idx;

    g_mutex_unlock(&base->graphic.surface_lock);

    struct timeval st;
    struct timeval ed;
    double time_total;
    gettimeofday(&st, NULL);

    GstMapInfo minfo;
    if (!gst_memory_map(base->graphic.render[render_idx].memory, &minfo, GST_MAP_WRITE)) {
      GST_ERROR_OBJECT(self, "failed to map new dma buffer");
      goto loop_continue;
    }

    GFX_Buf topBuf;
    topBuf.fd = base->graphic.render[render_idx].fd;
    topBuf.format = gfx_convert_video_format(TEMP_SURFACE_COLOR_FORMAT);
    topBuf.is_ionbuf = gst_is_amlionbuf_memory(base->graphic.render[render_idx].memory);
    topBuf.size.w = info->width;
    topBuf.size.h = info->height;

    // // Fill the rectangle with transparent color in the graphic.temp.fd
    GFX_Rect *pDirtyRect = &base->graphic.dirtyRect[render_idx];
    // //gfx_fillrect(handle, &topBuf, pDirtyRect, 0, 1);
    gfx_fillrect_software(handle, &topBuf, minfo.data, pDirtyRect, 0x0);

    // GFX_Rect rect = {0, 0, info->width, info->height};
    // gfx_fillrect_software(handle, &topBuf, minfo.data, &rect, 0x0);

    // clean the new render buffer dirty area
    memset(pDirtyRect, 0, sizeof(GFX_Rect));

    for (gint i = 0; i<rect_count; i++) {
      GFX_Rect small_rect = {pNNData[i].rect.x,
                            pNNData[i].rect.y,
                            pNNData[i].rect.w,
                            pNNData[i].rect.h};

      GST_INFO_OBJECT(self, "[%d] pRects(%d %d %d %d), id:%d, label_name:%s",
        i, pNNData[i].rect.x, pNNData[i].rect.y, pNNData[i].rect.w, pNNData[i].rect.h,
        pNNData[i].label_id, pNNData[i].label_name);

      gfx_updateDirtyArea(&topBuf, pDirtyRect, &small_rect);

      // Draw the border rectangle in the graphic.temp.fd
      gfx_drawrect_software(handle, &topBuf, minfo.data, &small_rect, self->nn.rectcolor, DEFAULT_PROP_RECT_THICKNESS);

      // draw point
      // for (gint j = 0; j<5; j++) {
      //   GFX_Rect posRect = {pNNData[i].pos[j].x,
      //                   pNNData[i].pos[j].y,
      //                   DEFAULT_PROP_RECT_THICKNESS,
      //                   DEFAULT_PROP_RECT_THICKNESS};
      //   gfx_drawrect_software(handle, &topBuf, minfo.data, &posRect, self->nn.rectcolor, DEFAULT_PROP_RECT_THICKNESS);
      // }
    }
    // memcpy(pDirtyRect, &rect, sizeof(GFX_Rect));

    gst_memory_unmap(base->graphic.render[render_idx].memory, &minfo);

    GST_INFO_OBJECT(self, "pDirtyRect(%d %d %d %d)", pDirtyRect->x, pDirtyRect->y, pDirtyRect->w, pDirtyRect->h);

    gettimeofday(&ed, NULL);
    time_total = (ed.tv_sec - st.tv_sec)*1000000.0 + (ed.tv_usec - st.tv_usec);

    // update new render buffer
    g_mutex_lock(&base->graphic.surface_lock);
    base->graphic.next_display_idx = base->graphic.render_idx;
    GST_INFO_OBJECT(self, "surface draw end, cur_display_idx=%d next_display_idx=%d render_idx=%d, time=%lf uS",
        base->graphic.cur_display_idx, base->graphic.next_display_idx, base->graphic.render_idx, time_total);
    g_mutex_unlock(&base->graphic.surface_lock);

loop_continue:

    if (NULL != pNNData) {
      g_free(pNNData);
      pNNData = NULL;
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

  }

  GST_INFO_OBJECT(self, "Leave");

  return NULL;
}


