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

#ifndef __GST_AMLNNOVERLAY_H__
#define __GST_AMLNNOVERLAY_H__

#include "gstamlbaseoverlay.h"

G_BEGIN_DECLS

#define GST_TYPE_AMLNNOVERLAY (gst_aml_nn_overlay_get_type())
#define GST_AMLNNOVERLAY(obj)                                                  \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AMLNNOVERLAY, GstAmlNNOverlay))
#define GST_AMLNNOVERLAY_CLASS(klass)                                          \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AMLNNOVERLAY,                     \
                           GstAmlNNOverlayClass))
#define GST_IS_AMLNNOVERLAY(obj)                                               \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AMLNNOVERLAY))
#define GST_IS_AMLNNOVERLAY_CLASS(klass)                                       \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AMLNNOVERLAY))

typedef struct _GstAmlNNOverlay GstAmlNNOverlay;
typedef struct _GstAmlNNOverlayClass GstAmlNNOverlayClass;

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
  char info[FACE_INFO_BUFSIZE];   // string
};

struct nn_result_buffer {
  gint amount; // amount of result
  struct nn_result *results;
};

#define NN_MAX_RECT 20
struct face_rects {
  struct aml_overlay_surface surface;
  struct aml_overlay_location location;
};

struct _GstAmlNNOverlay {
  GstAmlOverlay element;

  /*< private >*/
  struct {
    gboolean enabled;
    struct aml_overlay_font font;
    guint rectcolor;
    struct aml_overlay_surface hfont;
    struct {
      struct nn_result_buffer *buf;

      // lock result data and surface opt
      GMutex lock;
      guint srcid;
    } result;
  } nn;
};

struct _GstAmlNNOverlayClass {
  GstAmlOverlayClass parent_class;
};

GType gst_aml_nn_overlay_get_type(void);

G_END_DECLS

#endif /* __GST_AMLNNOVERLAY_H__ */
