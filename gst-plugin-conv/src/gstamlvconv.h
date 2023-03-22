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

#ifndef __GST_AMLVCONV_H__
#define __GST_AMLVCONV_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include <aml_ge2d.h>

G_BEGIN_DECLS



#define GST_TYPE_AML_ROTATION(module) (gst_aml_##module##_rotation_get_type())

#define GST_DECLARE_AML_ROTATION_GET_TYPE(module)                              \
  GType gst_aml_##module##_rotation_get_type(void)

#define GST_DEFINE_AML_ROTATION_GET_TYPE(module)                               \
  GType gst_aml_##module##_rotation_get_type(void) {                           \
    static GType aml_##module##_rotation_type = 0;                             \
    static const GEnumValue aml_##module##_rotation[] = {                      \
        {GST_AML_ROTATION_0, "0", "rotate 0 degrees"},                         \
        {GST_AML_ROTATION_90, "90", "rotate 90 degrees"},                      \
        {GST_AML_ROTATION_180, "180", "rotate 180 degrees"},                   \
        {GST_AML_ROTATION_270, "270", "rotate 270 degrees"},                   \
        {0, NULL, NULL},                                                       \
    };                                                                         \
    if (!aml_##module##_rotation_type) {                                       \
      aml_##module##_rotation_type = g_enum_register_static(                   \
          "GstAML" #module "Rotation", aml_##module##_rotation);               \
    }                                                                          \
    return aml_##module##_rotation_type;                                       \
  }


#define GST_TYPE_AMLVCONV \
  (gst_aml_vconv_get_type())
#define GST_AMLVCONV(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AMLVCONV,GstAmlVConv))
#define GST_AMLVCONV_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AMLVCONV,GstAmlVConvClass))
#define GST_IS_AMLVCONV(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AMLVCONV))
#define GST_IS_AMLVCONV_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AMLVCONV))

typedef struct _GstAmlVConv      GstAmlVConv;
typedef struct _GstAmlVConvClass GstAmlVConvClass;


struct _GstAmlVConv {
  GstVideoFilter element;

  /*< private >*/
  struct imgproc_info {
    void *handle;
    // GstAmlRotation rotation;
    gint rotation;
  } imgproc;

  GstAllocator *dmabuf_alloc;

  gboolean is_info_set;
  GstVideoInfo in_info;
  GstVideoInfo out_info;

  /* properties */

};

struct _GstAmlVConvClass {
  GstVideoFilterClass parent_class;
};

GType gst_aml_vconv_get_type (void);

GST_DECLARE_AML_ROTATION_GET_TYPE(vconv);

G_END_DECLS

#endif /* __GST_AMLVCONV_H__ */
