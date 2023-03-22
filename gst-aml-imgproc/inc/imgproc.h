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
#ifndef _FRAMERESIZE_H
#define _FRAMERESIZE_H
#include <gst/gst.h>
#include <gst/video/video.h>

typedef enum {
  GST_AML_ROTATION_0,
  GST_AML_ROTATION_90,
  GST_AML_ROTATION_180,
  GST_AML_ROTATION_270,
  GST_AML_MIRROR_X,
  GST_AML_MIRROR_Y,
} GstAmlRotation;


gint convert_video_format(GstVideoFormat format);
gint convert_video_rotation(GstAmlRotation rotation);


struct imgproc_buf {
  gint fd;
  gboolean is_ionbuf;
};

struct imgproc_pos {
  gint x;
  gint y;
  gint w;
  gint h;
  gint canvas_w;
  gint canvas_h;
};

void* imgproc_init ();
void imgproc_deinit (void *handle);
gboolean imgproc_crop (void* handle,
    struct imgproc_buf in_buf,
    struct imgproc_pos in_pos, GstVideoFormat in_format,
    struct imgproc_buf out_buf,
    struct imgproc_pos out_pos, GstVideoFormat out_format);
gboolean imgproc_transform(void *handle, struct imgproc_buf in_buf,
                           struct imgproc_pos in_pos, GstVideoFormat in_format,
                           struct imgproc_buf out_buf,
                           struct imgproc_pos out_pos,
                           GstVideoFormat out_format, GstAmlRotation rotation);
gboolean imgproc_fillrect(void *handle, GstVideoFormat format,
                          struct imgproc_buf buf, struct imgproc_pos pos,
                          guint color);

#endif /* _FRAMERESIZE_H */
