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
 * SECTION:element-amlvconv
 *
 * FIXME:Describe amlvconv here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! amlvconv ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <unistd.h>

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/video/video.h>
#include <gst/controller/controller.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/allocators/gstamlionallocator.h>

#include "gstamlvconv.h"

#include "imgproc.h"
// #include "gfx_2d.h"
// #include "gst_ge2d.h"


#define GST_TYPE_AMLVCONV_ROTATION (GST_TYPE_AML_ROTATION(vconv))


GST_DEBUG_CATEGORY_STATIC (gst_aml_vconv_debug);
#define GST_CAT_DEFAULT gst_aml_vconv_debug

GST_DEFINE_AML_ROTATION_GET_TYPE(vconv);

enum
{
  PROP_0,
  PROP_ROTATION,
};

#define DEFAULT_PROP_ROTATION GST_AML_ROTATION_0

/* the capabilities of the inputs and outputs.
 *
 */
#define GST_VIDEO_FORMATS "{" \
  " RGB, RGBA, ARGB, RGBx, RGB16," \
  " BGR, BGRA, ABGR," \
  " YV12, NV16, NV21, UYVY, NV12," \
  " I420" \
  " } "


static GstStaticCaps gst_aml_vconv_format_caps =
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS) ";"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY", GST_VIDEO_FORMATS));

#define gst_aml_vconv_parent_class parent_class
G_DEFINE_TYPE (GstAmlVConv, gst_aml_vconv, GST_TYPE_VIDEO_FILTER);

static void gst_aml_vconv_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_aml_vconv_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_aml_vconv_open (GstBaseTransform *trans);

static gboolean gst_aml_vconv_close (GstBaseTransform *trans);

static void gst_aml_vconv_finalize (GObject * object);

static GstCaps *
gst_aml_vconv_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);

static gboolean gst_aml_vconv_set_info (GstVideoFilter * filter,
    GstCaps * in, GstVideoInfo * in_info, GstCaps * out,
    GstVideoInfo * out_info);
static GstFlowReturn gst_aml_vconv_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * in, GstVideoFrame * out);
static gboolean gst_aml_vconv_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);

/* GObject vmethod implementations */
static GstCaps *
gst_aml_vconv_get_capslist (void)
{
  static GstCaps *caps = NULL;
  static volatile gsize inited = 0;

  if (g_once_init_enter (&inited)) {
    caps = gst_static_caps_get (&gst_aml_vconv_format_caps);
    g_once_init_leave (&inited, 1);
  }
  return caps;
}

static GstPadTemplate *
gst_aml_vconv_src_template_factory (void)
{
  return gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      gst_aml_vconv_get_capslist ());
}

static GstPadTemplate *
gst_aml_vconv_sink_template_factory (void)
{
  return gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      gst_aml_vconv_get_capslist ());
}

/* copies the given caps */
static GstCaps *
gst_aml_vconv_caps_remove_format_info (GstCaps * caps)
{
  GstStructure *st;
  GstCapsFeatures *f;
  gint i, n;
  GstCaps *res;

  res = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    st = gst_caps_get_structure (caps, i);
    f = gst_caps_get_features (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure_full (res, st, f))
      continue;

    st = gst_structure_copy (st);
    /* Only remove format info for the cases when we can actually convert */
    if (!gst_caps_features_is_any (f)
        && gst_caps_features_is_equal (f,
            GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY))
      gst_structure_remove_fields (st,
          "format", "colorimetry", "chroma-site",
          "width", "height",
          NULL);

    gst_caps_append_structure_full (res, st, gst_caps_features_copy (f));
  }

  return res;
}

static GstCaps *
gst_aml_vconv_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *tmp, *tmp2;
  GstCaps *result;

  /* Get all possible caps that we can transform to */
  tmp = gst_aml_vconv_caps_remove_format_info (caps);

  if (filter) {
    tmp2 = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
    tmp = tmp2;
  }

  result = tmp;

  GST_DEBUG_OBJECT (trans, "transformed %" GST_PTR_FORMAT " into %"
      GST_PTR_FORMAT, caps, result);

  return result;
}

static void gst_aml_vconv_set_passthrough(GstAmlVConv *vconv) {
  GstVideoInfo *in_info = &vconv->in_info;
  GstVideoInfo *out_info = &vconv->out_info;

  if (!vconv->is_info_set) {
    return;
  }

  if (in_info->width == out_info->width &&
      in_info->height == out_info->height &&
      GST_VIDEO_INFO_FORMAT(in_info) == GST_VIDEO_INFO_FORMAT(out_info) &&
      vconv->imgproc.rotation == GST_AML_ROTATION_0) {
    gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(vconv), TRUE);
  } else {
    gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(vconv), FALSE);
  }

  GST_INFO_OBJECT(
      vconv,
      "from=%dx%d (format=%d), size %" G_GSIZE_FORMAT
      " -> to=%dx%d (format=%d), size %" G_GSIZE_FORMAT
      ", rotation=%d",
      in_info->width, in_info->height, GST_VIDEO_INFO_FORMAT(in_info), in_info->size,
      out_info->width, out_info->height, GST_VIDEO_INFO_FORMAT(out_info), out_info->size,
      vconv->imgproc.rotation);
}

static gboolean gst_aml_vconv_set_info(GstVideoFilter *filter, GstCaps *in,
                                       GstVideoInfo *in_info, GstCaps *out,
                                       GstVideoInfo *out_info) {
  GstAmlVConv *vconv = GST_AMLVCONV(filter);

  memcpy(&vconv->in_info, in_info, sizeof(GstVideoInfo));
  memcpy(&vconv->out_info, out_info, sizeof(GstVideoInfo));
  vconv->is_info_set = TRUE;

  gst_aml_vconv_set_passthrough(vconv);

  return TRUE;
}

static GstFlowReturn
gst_aml_vconv_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer ** outbuf)
{
  GstFlowReturn ret;
  GstBaseTransformClass *bclass;
  GstAmlVConv *vconv = GST_AMLVCONV (trans);
  GstVideoFilter *filter = &vconv->element;

  bclass = GST_BASE_TRANSFORM_GET_CLASS (trans);

  /* figure out how to allocate an output buffer */
  if (gst_base_transform_is_passthrough (trans)) {
    /* passthrough, we will not modify the incoming buffer so we can just
     * reuse it */
    GST_DEBUG_OBJECT (trans, "passthrough: reusing input buffer");
    *outbuf = inbuf;
    goto done;
  }

  if ((bclass->transform_ip != NULL) && gst_base_transform_is_in_place (trans)) {
    /* we want to do an in-place alloc */
    if (gst_buffer_is_writable (inbuf)) {
      GST_DEBUG_OBJECT (trans, "inplace reuse writable input buffer");
      *outbuf = inbuf;
    } else {
      GST_DEBUG_OBJECT (trans, "making writable buffer copy");
      /* we make a copy of the input buffer */
      *outbuf = gst_buffer_copy (inbuf);
    }
    goto done;
  }

  *outbuf = gst_buffer_new ();
  if (!*outbuf) {
    ret = GST_FLOW_ERROR;
    goto alloc_failed;
  }

  if (vconv->dmabuf_alloc == NULL) {
    vconv->dmabuf_alloc = gst_amlion_allocator_obtain();
  }


  GstMemory *memory =
      gst_allocator_alloc(vconv->dmabuf_alloc, filter->out_info.size, NULL);
  gst_buffer_insert_memory(*outbuf, -1, memory);

  /* copy the metadata */
  if (bclass->copy_metadata)
    if (!bclass->copy_metadata (trans, inbuf, *outbuf)) {
      /* something failed, post a warning */
      GST_ELEMENT_WARNING (trans, STREAM, NOT_IMPLEMENTED,
          ("could not copy metadata"), (NULL));
    }

done:
  return GST_FLOW_OK;

  /* ERRORS */
alloc_failed:
  {
    GST_DEBUG_OBJECT (trans, "could not allocate buffer from pool");
    return ret;
  }
}

static GstFlowReturn
gst_aml_vconv_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
  GstAmlVConv *self = GST_AMLVCONV (filter);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT(self, "process begin");
  if (self->imgproc.handle == NULL) {
    self->imgproc.handle = imgproc_init();
    if (self->imgproc.handle == NULL) {
      GST_ELEMENT_ERROR(filter, CORE, NEGOTIATION, (NULL),
                        ("imgproc not initialized"));
      return GST_FLOW_NOT_NEGOTIATED;
    }
  }

  struct imgproc_buf inbuf, outbuf;
  GstMemory *out_memory = gst_buffer_get_memory (out_frame->buffer, 0);
  if (gst_is_dmabuf_memory (out_memory)) {
    outbuf.fd = gst_dmabuf_memory_get_fd (out_memory);
    outbuf.is_ionbuf = TRUE;
  }
  if (outbuf.fd < 0) {
    GST_ERROR_OBJECT (self, "unexpected error: output should be dmabuffer");
    ret = GST_FLOW_ERROR;
    return ret;
  }

  GstMemory *in_memory = gst_buffer_get_memory (in_frame->buffer, 0);

  if (self->dmabuf_alloc == NULL) {
    self->dmabuf_alloc = gst_amlion_allocator_obtain();
  }

  if (gst_is_dmabuf_memory (in_memory)) {
    inbuf.fd = gst_dmabuf_memory_get_fd (in_memory);
  } else {
    gst_memory_unref (in_memory);
    in_memory =
      gst_allocator_alloc(self->dmabuf_alloc, filter->in_info.size, NULL);
    if (in_memory) {
      GstMapInfo minfo;
      inbuf.fd = gst_dmabuf_memory_get_fd (in_memory);
      if (gst_memory_map (in_memory, &minfo, GST_MAP_WRITE)) {
        guint8 *pixels = GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0);
        memcpy (minfo.data, pixels, minfo.size);
      }
      gst_memory_unmap (in_memory, &minfo);
    }
  }

  if (inbuf.fd < 0) {
    GST_ERROR_OBJECT (self, "failed to obtain the input memory fd");
    ret = GST_FLOW_ERROR;
    goto transform_end;
  }

  inbuf.is_ionbuf = gst_is_amlionbuf_memory(in_memory);

  struct imgproc_pos inpos = {0,
                              0,
                              filter->in_info.width,
                              filter->in_info.height,
                              filter->in_info.width,
                              filter->in_info.height};
  struct imgproc_pos outposition = {0,
                               0,
                               filter->out_info.width,
                               filter->out_info.height,
                               filter->out_info.width,
                               filter->out_info.height};

  if (!imgproc_transform(self->imgproc.handle, inbuf, inpos,
                         GST_VIDEO_INFO_FORMAT(&filter->in_info), outbuf,
                         outposition, GST_VIDEO_INFO_FORMAT(&filter->out_info),
                         self->imgproc.rotation)) {
    GST_ERROR_OBJECT(self, "failed to finish imgproc");
    goto transform_end;
  }

  if (GST_VIDEO_INFO_FORMAT (&filter->out_info) == GST_VIDEO_FORMAT_I420) {
    guint8 *pixels = GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0);
    gint size_y = filter->out_info.width * filter->out_info.height;
    gint size_uv = size_y >> 1;
    gint size_u = size_uv >> 1;

    guint8 *pixel_uv_bak = g_new (guint8, size_uv);

    guint8 *pixel_u = pixels + size_y;
    guint8 *pixel_v = pixel_u + size_u;

    // backup UV
    memcpy (pixel_uv_bak, pixel_u, size_uv);

    // YYYY UVUV -> YYYY UU VV
    for (gint i = 0; i < size_u; i++) {
      pixel_u[i] = pixel_uv_bak[i * 2];
      pixel_v[i] = pixel_uv_bak[i * 2 + 1];
    }

    g_free (pixel_uv_bak);
  }

transform_end:
  if (out_memory) {
    gst_memory_unref (out_memory);
  }
  if (in_memory != NULL) {
    gst_memory_unref (in_memory);
  }

  GST_DEBUG_OBJECT(self, "process end");

  return ret;
}

static gboolean
gst_aml_vconv_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstVideoInfo info;
  GstCaps *caps;
  guint size, min = 0, max = 0;

  gst_query_parse_allocation (query, &caps, NULL);

  if (caps == NULL)
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;

  gint n = gst_query_get_n_allocation_pools (query);
  if (n > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, NULL, &size, &min, &max);
    size = MAX (size, info.size);
    gst_query_set_nth_allocation_pool (query, 0, NULL, size, 4, 6);
  } else {
    gst_query_add_allocation_pool (query, NULL, info.size, 4, 4);
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (trans, decide_query, query);
}


/* initialize the amlvconv's class */
static void
gst_aml_vconv_class_init (GstAmlVConvClass * klass)
{
  GstElementClass *element_class;

  element_class = (GstElementClass *) klass;

  G_OBJECT_CLASS(klass)->set_property = gst_aml_vconv_set_property;
  G_OBJECT_CLASS(klass)->get_property = gst_aml_vconv_get_property;
  G_OBJECT_CLASS(klass)->finalize = gst_aml_vconv_finalize;

  g_object_class_install_property(
      G_OBJECT_CLASS(klass), PROP_ROTATION,
      g_param_spec_enum("rotation", "rotation", "video rotation",
                        GST_TYPE_AMLVCONV_ROTATION, DEFAULT_PROP_ROTATION,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  gst_element_class_set_details_simple (element_class,
    "Amlogic Video Converter",
    "Filter/Editor/Video",
    "Video resize, color space transform module",
    "Jemy Zhang <jun.zhang@amlogic.com>");

  gst_element_class_add_pad_template (element_class,
      gst_aml_vconv_sink_template_factory ());
  gst_element_class_add_pad_template (element_class,
      gst_aml_vconv_src_template_factory ());

  GST_BASE_TRANSFORM_CLASS (klass)->start =
    GST_DEBUG_FUNCPTR (gst_aml_vconv_open);
  GST_BASE_TRANSFORM_CLASS (klass)->stop =
    GST_DEBUG_FUNCPTR (gst_aml_vconv_close);
  GST_BASE_TRANSFORM_CLASS (klass)->transform_caps =
    GST_DEBUG_FUNCPTR (gst_aml_vconv_transform_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->prepare_output_buffer =
    GST_DEBUG_FUNCPTR (gst_aml_vconv_prepare_output_buffer);

  GST_VIDEO_FILTER_CLASS (klass)->set_info =
    GST_DEBUG_FUNCPTR (gst_aml_vconv_set_info);
  GST_VIDEO_FILTER_CLASS (klass)->transform_frame =
    GST_DEBUG_FUNCPTR (gst_aml_vconv_transform_frame);

  GST_BASE_TRANSFORM_CLASS (klass)->propose_allocation =
    GST_DEBUG_FUNCPTR (gst_aml_vconv_propose_allocation);
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_aml_vconv_init (GstAmlVConv *vconv)
{
  vconv->is_info_set = FALSE;
  vconv->imgproc.handle = NULL;
  vconv->imgproc.rotation = DEFAULT_PROP_ROTATION;
  vconv->dmabuf_alloc = NULL;
}

static void
gst_aml_vconv_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAmlVConv *self = GST_AMLVCONV (object);
  switch (prop_id) {
  case PROP_ROTATION:
    self->imgproc.rotation = g_value_get_enum(value);
    gst_aml_vconv_set_passthrough(self);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void
gst_aml_vconv_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAmlVConv *self = GST_AMLVCONV (object);
  switch (prop_id) {
  case PROP_ROTATION:
    g_value_set_enum(value, self->imgproc.rotation);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean
gst_aml_vconv_open (GstBaseTransform *trans)
{
  return TRUE;
}

static gboolean
gst_aml_vconv_close (GstBaseTransform *trans)
{
  GstAmlVConv *self = GST_AMLVCONV (trans);
  if (self->imgproc.handle != NULL) {
    imgproc_deinit (self->imgproc.handle);
    self->imgproc.handle = NULL;
  }
  if (self->dmabuf_alloc) {
    gst_object_unref (self->dmabuf_alloc);
    self->dmabuf_alloc = NULL;
  }
  return TRUE;
}

static void
gst_aml_vconv_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
amlvconv_init (GstPlugin * amlvconv)
{
  GST_DEBUG_CATEGORY_INIT (gst_aml_vconv_debug, "amlvconv", 0,
      "amlogic vconv");

  return gst_element_register (amlvconv, "amlvconv", GST_RANK_PRIMARY,
      GST_TYPE_AMLVCONV);
}

/* gstreamer looks for this structure to register amlvconvs
 *
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    amlvconv,
    "Amlogic video convert",
    amlvconv_init,
    VERSION,
    "LGPL",
    "Amlogic gstreamer plugins",
    "http://openlinux.amlogic.com"
)
