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

#ifndef __GST_AMLBASEOVERLAY_H__
#define __GST_AMLBASEOVERLAY_H__

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "gfx_2d.h"
#include "gst_ge2d.h"

// for performance measure
#include "sys/time.h"

G_BEGIN_DECLS

#define GST_TYPE_AMLOVERLAY (gst_aml_overlay_get_type())
#define GST_AMLOVERLAY(obj)                                                    \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AMLOVERLAY, GstAmlOverlay))
#define GST_AMLOVERLAY_CLASS(klass)                                            \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AMLOVERLAY, GstAmlOverlayClass))
#define GST_IS_AMLOVERLAY(obj)                                                 \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AMLOVERLAY))
#define GST_IS_AMLOVERLAY_CLASS(klass)                                         \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AMLOVERLAY))
#define GST_AMLOVERLAY_CAST(obj) ((GstAmlOverlay *)(obj))

typedef struct _GstAmlOverlay GstAmlOverlay;
typedef struct _GstAmlOverlayClass GstAmlOverlayClass;


#define AMLOVERLAY_FREE_STRING(s)                                              \
  do {                                                                         \
    if (s) {                                                                   \
      g_free(s);                                                               \
      s = NULL;                                                                \
    }                                                                          \
  } while (0)

#define GST_TYPE_AMLOVERLAY_POS (gst_aml_overlay_pos_get_type())




#define DEFAULT_PROP_FONTCOLOR 0x00ffffff

#define DEFAULT_PROP_RECTCOLOR 0xff0000ff
#define TEMP_SURFACE_COLOR_FORMAT GST_VIDEO_FORMAT_RGBA



typedef enum {
  GST_AMLOVERLAY_POS_TOP_LEFT,
  GST_AMLOVERLAY_POS_TOP_MID,
  GST_AMLOVERLAY_POS_TOP_RIGHT,
  GST_AMLOVERLAY_POS_MID_LEFT,
  GST_AMLOVERLAY_POS_MID_RIGHT,
  GST_AMLOVERLAY_POS_CENTER,
  GST_AMLOVERLAY_POS_BOTTOM_LEFT,
  GST_AMLOVERLAY_POS_BOTTOM_MID,
  GST_AMLOVERLAY_POS_BOTTOM_RIGHT,
} GstAmlOverlayPOS;

struct aml_overlay_location {
  gint left;
  gint top;
  gint width;
  gint height;
};

struct aml_overlay_font {
  gint size;
  guint fgcolor;
  guint bgcolor;
  gchar *ttf;
};

struct aml_overlay_surface {
  guint srcid;
  void *handle;
  GMutex *mutex;
};

typedef enum BufferType {
  AMLOVERLAY_DMABUF = 0,
  AMLOVERLAY_USRBUF = 0xff,
}AmlOverlayBufType;



#define RENDER_BUF_CNT 2


typedef gpointer (*work_process)(void *data);


struct _GstAmlOverlay {
  GstBaseTransform element;

  /*< private >*/
  GThread *m_thread;
  work_process process;
  GstAllocator *dmabuf_alloc;

  // memories for graphic transaction
  // input memory only used under non-dmabuf input situation
  struct {
    struct {
      GstMemory *memory;
      gint fd;
      gint size;
    } m_input, m_render[RENDER_BUF_CNT], m_output;

    // The dirtyRect of render buffer
    GFX_Rect dirtyRect[RENDER_BUF_CNT];

    // for display render buffer (render_idx/display_idx)
    GMutex surface_lock;

    // current render buffer index
    int render_idx;
    int cur_display_idx;
    int next_display_idx;

    // gfx2d handle
    void *m_gfxhandle;
    gint width;
    gint height;
    gint size;
  } graphic;

  AmlOverlayBufType inputbuf_type;
  GstVideoInfo info;
  gboolean is_info_set;
};

struct _GstAmlOverlayClass {
  GstBaseTransformClass parent_class;
};

GType gst_aml_overlay_get_type(void);

GType gst_aml_overlay_pos_get_type(void);
void gst_aml_overlay_init_surface_struct(struct aml_overlay_surface *data,
                                         GMutex *m);
void gst_aml_overlay_deinit_surface_struct(struct aml_overlay_surface *data);
gboolean gst_aml_overlay_allocate_memory(GstBaseTransform *trans, gint width,
                                         gint height);
void gst_aml_overlay_calc_text_pos(gint string_width, gint font_height,
                                   gint width, gint height,
                                   GstAmlOverlayPOS pos, gint *x, gint *y);

G_END_DECLS

#endif /* __GST_AMLBASEOVERLAY_H__ */
