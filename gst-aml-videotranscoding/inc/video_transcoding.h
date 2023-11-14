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

#ifndef _VIDEO_TRANSCODING_H_
#define _VIDEO_TRANSCODING_H_

#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include <pthread.h>

typedef enum {
    AV1, //0
    AVS2,
    AVS3,
    AVS,
    H264, //video/x-h264
    H265, //video/x-h265
    JPEG, //image/jpeg
    MPEG4, //video/mpeg
    VC1,
    VP9,
    INVALID_CODEC
}CodecType;

//use gstreamer enum
typedef void* HANDLE;
typedef int (*video_transcoding_callback)(HANDLE *handle, void *buffer, gint size);

typedef struct _CustomData {
  GstElement *pipeline, *app_source, *v4l2_dec, *app_sink;
  GstElement *video_convert, *video_sink, *video_enc, *video_rate;

  guint sourceid;        /* To control the GSource */
  gboolean playing;      /* media player playing flag */
  gboolean init;        /* media player init flag */

  HANDLE handle;
  pthread_t tid;

  GMainLoop *main_loop;  /* GLib's Main Loop */
  video_transcoding_callback cus_pull_data_callback;
} CustomData;


// typedef struct _transcoding_pip {
//     HANDLE handle;
//     pthread_t tid;
// }transcoding_pip;

typedef struct _video_size
{
  gint width;
  gint height;
  //gint size;
}video_size;

typedef struct _video_transcoding_param
{
video_size src_size;
CodecType src_codec;
int src_framerate;

video_size dst_size;
CodecType dst_codec;
int dst_framerate;
int bitrate_kb;
int gop_size;
}video_transcoding_param;


//typedef GstFlowReturn (*video_transcoding_callback)(GstElement *sink, CustomData *data);
HANDLE video_transcoding_init(video_transcoding_param *param, int argc, char **argv);
void video_transcoding_deinit(HANDLE *handle);
//int video_transcoding_writeData(HANDLE handle, void *pData);
// void *video_transcoding_workloop(HANDLE *handle);
int video_transcoding_start(HANDLE *handle);
int video_transcoding_stop(HANDLE *handle);
int video_transcoding_writeData(HANDLE handle, void* buffer, gint buff_size);
int video_transcoding_setSinkCallback(HANDLE *handle, video_transcoding_callback cb);

#endif
