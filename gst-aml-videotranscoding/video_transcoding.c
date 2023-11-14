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

#include <gst/gst.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include "video_transcoding.h"

GST_DEBUG_CATEGORY_STATIC(video_transcoding);
#define GST_CAT_DEFAULT video_transcoding



//GstFlowReturn video_transcoding_pulldata_callback(GstElement *sink, CustomData *data);
static void *video_transcoding_workloop(HANDLE *handle);

static GstFlowReturn appsink_pull_data_callback(GstElement *sink, CustomData *data );

HANDLE video_transcoding_init( video_transcoding_param *param, int argc, char **argv){
    GstCaps* src_caps = NULL;
    GstCaps* sink_caps = NULL;


    CustomData *data = (CustomData *)malloc(sizeof(CustomData));
    if (NULL == data)
    {
        GST_ERROR("%s malloc CustomData error\n", __func__);
        return NULL;
    }

    /* init the logsystem  */
    GST_DEBUG_CATEGORY_EXTERN(video_transcoding);
    GST_DEBUG_CATEGORY_INIT(video_transcoding, "video_transcoding", 0, "libtranscoding log");

    GST_DEBUG("%s src_width=%d src_height=%d  src_codec=%d src_framerate=%d/1\n \
                  dst_width=%d dst_height=%d  dst_codec=%d dst_framerate=%d/1\n \
                  bitrate_kb=%d gop_size=%d\n",
                  __func__, param->src_size.width, param->src_size.height, param->src_codec, param->src_framerate,
                  param->dst_size.width, param->dst_size.height, param->dst_codec, param->dst_framerate,
                  param->bitrate_kb, param->gop_size);

    printf("%s src_width=%d src_height=%d  src_codec=%d src_framerate=%d/1\n \
                dst_width=%d dst_height=%d dst_codec=%d dst_framerate=%d/1\n \
                bitrate_kb=%d gop_size=%d\n",
                __func__, param->src_size.width, param->src_size.height, param->src_codec, param->src_framerate,
                param->dst_size.width, param->dst_size.height, param->dst_codec, param->dst_framerate,
                param->bitrate_kb, param->gop_size);

    /* Initialize cumstom data structure */
    memset (data, 0, sizeof (CustomData));

    /* Initialize GStreamer */
    gst_init (&argc, &argv);

    /* switch the decoder codec */
    switch (param->src_codec)
    {
    case AV1:
        data->v4l2_dec = gst_element_factory_make ("amlv4l2av1dec", "v4l2_av1_dec");
        break;
    case AVS2:
        data->v4l2_dec = gst_element_factory_make ("amlv4l2avs2dec", "v4l2_avs2_dec");
        break;
    case AVS3:
        data->v4l2_dec = gst_element_factory_make ("amlv4l2avs3dec", "v4l2_avs3_dec");
        break;
    case AVS:
        data->v4l2_dec = gst_element_factory_make ("amlv4l2avsdec", "v4l2_avs_dec");
        break;
    case H264:
        data->v4l2_dec = gst_element_factory_make ("amlv4l2h264dec", "v4l2_h264_dec");
        src_caps = gst_caps_new_simple("video/x-h264",
                                        "width", G_TYPE_INT, param->src_size.width,
                                        "height", G_TYPE_INT, param->src_size.height,
                                        "framerate", GST_TYPE_FRACTION, param->src_framerate,
                                        NULL);
        break;
    case H265:
        data->v4l2_dec = gst_element_factory_make ("amlv4l2h265dec", "v4l2_h265_dec");
        src_caps = gst_caps_new_simple("video/x-h265",
                                        "width", G_TYPE_INT, param->src_size.width,
                                        "height", G_TYPE_INT, param->src_size.height,
                                        "framerate", GST_TYPE_FRACTION, param->src_framerate,
                                        NULL);
        break;
    case JPEG:
        data->v4l2_dec = gst_element_factory_make ("amlv4l2jpegdec", "v4l2_jpeg_dec");
        break;
    case MPEG4:
        data->v4l2_dec = gst_element_factory_make ("amlv4l2mpeg4dec", "v4l2_mpeg4_dec");
        src_caps = gst_caps_new_simple("video/mpeg",
                                        "mpegversion", G_TYPE_INT, 2,
                                        "systemstream", G_TYPE_BOOLEAN, FALSE,
                                        // "width", G_TYPE_INT, param->src_size.width,
                                        // "height", G_TYPE_INT, param->src_size.height,
                                        // "framerate", GST_TYPE_FRACTION, param->src_framerate,
                                        NULL);

        //src_caps = gst_caps_new_simple("video/mpeg",NULL);
        //src_caps = gst_caps_new_any();
        break;
    case VC1:
        data->v4l2_dec = gst_element_factory_make ("amlv4l2vc1dec", "v4l2_vc1_dec");
        break;
    case VP9:
        data->v4l2_dec = gst_element_factory_make ("amlv4l2vp9dec", "v4l2_vp9_dec");
        break;
    default:
        GST_ERROR("%s No match dec codec : %d\n", __func__, param->src_codec);
        break;
    }

    //const gchar *dec_name = gst_element_get_name(data->v4l2_dec);
    GST_DEBUG("%s dec plugin name : %s\n", __func__, gst_element_get_name(data->v4l2_dec));

    /* switch the encoder codec */
    switch (param->dst_codec)
    {
    case JPEG:
        data->video_enc = gst_element_factory_make ("amljpegenc", "video_jpeg_enc");
        break;
    case H264:
        data->video_enc = gst_element_factory_make ("amlvenc", "video_h264_enc");
                    //sink_caps = gst_caps_new_simple("video/x-raw",
        sink_caps = gst_caps_new_simple("video/x-h264",
                                        "width", G_TYPE_INT, param->dst_size.width,
                                        "height", G_TYPE_INT, param->dst_size.height,
                                        "framerate", GST_TYPE_FRACTION, param->dst_framerate,
                                        NULL);
        break;
    case H265:
        data->video_enc = gst_element_factory_make ("amlvenc", "video_h265_enc");
        sink_caps = gst_caps_new_simple("video/x-h265",
                                        "width", G_TYPE_INT, param->dst_size.width,
                                        "height", G_TYPE_INT, param->dst_size.height,
                                        "framerate", GST_TYPE_FRACTION, param->dst_framerate,1,
                                        NULL);
        break;
    default:
        GST_ERROR("No match enc codec : %d\n", param->dst_codec);
        break;
    }

    //const gchar *enc_name = gst_element_get_name(data->video_enc);
    GST_DEBUG("%s encoder plugin name : %s\n", __func__, gst_element_get_name(data->video_enc));

    /* Create the elements */
    // data->app_queue = gst_element_factory_make ("queue", "app_queue");
    data->app_sink = gst_element_factory_make ("appsink", "app_sink");
    data->app_source = gst_element_factory_make ("appsrc", "app_source");
    // data->ts_demux = gst_element_factory_make ("tsdemux", "ts_demux");
    data->video_convert = gst_element_factory_make ("amlvconv", "video_convert");
    // data->src_parse = gst_element_factory_make ("mpegvideoparse", "src_parse");
    // data->video_queue = gst_element_factory_make ("queue", "video_queue");
    //data->video_rate = gst_element_factory_make ("videorate", "video_rate");

    /* Create the empty pipeline */
    data->pipeline = gst_pipeline_new ("transcoding-pipeline");

    if (!data->pipeline || !data->app_sink || !data->app_source || !data->v4l2_dec || !data->video_convert || !data->video_enc) {
        GST_ERROR("%s Not all elements could be created.\n", __func__);
        return NULL;
    }

    /* configure the appsrc */
    g_object_set(data->app_source, "caps", src_caps, NULL);
    GST_DEBUG("%s sink_caps we set : %s\n",__func__, gst_caps_to_string(src_caps));
    gst_caps_unref(src_caps);

    //g_signal_connect (data->app_source, "need-data", G_CALLBACK (start_feed), &data);
    //g_signal_connect (data->app_source, "enough-data", G_CALLBACK (stop_feed), &data);

    /* configure the amlvenc */
    g_object_set(data->video_enc,"bitrate", param->bitrate_kb, "gop", param->gop_size, NULL);

    /*configure the app sink */
    g_object_set (data->app_sink, "emit-signals", TRUE, "caps", sink_caps, NULL);
    //"async", FALSE,
    GST_DEBUG("%s sink_caps we set : %s\n",__func__, gst_caps_to_string(sink_caps));
    gst_caps_unref(sink_caps);
    //g_signal_connect (data->app_sink, "new-sample", G_CALLBACK (appsink_pull_data_callback), data);

    /* Add all plugins to the pipeline */
    gst_bin_add_many (GST_BIN (data->pipeline), data->app_source, data->v4l2_dec, data->video_convert, data->video_enc,data->app_sink ,NULL);
    if (gst_element_link_many (data->app_source, data->v4l2_dec, data->video_convert, data->video_enc, data->app_sink, NULL) != TRUE ) {
        GST_ERROR("%s Elements could not be linked.\n", __func__);
        gst_object_unref (data->pipeline);
        return NULL;
    }

    /* Create a GLib Main Loop and set it to run */
    data->main_loop = g_main_loop_new (NULL, FALSE);
    data->init = TRUE;
    data->playing = FALSE;
    data->cus_pull_data_callback = NULL;

    // start thread
    pthread_create(&data->tid, NULL, video_transcoding_workloop, (HANDLE)data);

    return (HANDLE)data;

}

void video_transcoding_deinit(HANDLE *handle){
    /* chech input param */
    if (NULL == handle) {
        GST_ERROR("%s invalid param!\n", __func__);
        return;
    }
    CustomData *data = (CustomData *)handle;
    /* free resources */
    data->init = FALSE;
    data->playing = FALSE;
    g_main_loop_quit(data->main_loop);
    g_main_loop_unref(data->main_loop);
    gst_object_unref(data->pipeline);
    free(data);

    return;
}

/* This function is called when an error message is posted on the bus */
static gboolean handle_message (GstBus *bus, GstMessage *msg, CustomData *data)
{
    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_ERROR:
        gst_message_parse_error(msg, &err, &debug_info);
        GST_ERROR("%s Error received from element %s: %s\n",__func__, GST_OBJECT_NAME(msg->src), err->message);
        GST_ERROR("Debugging information: %s\n", debug_info ? debug_info : "none");
        g_clear_error(&err);
        g_free(debug_info);
        g_main_loop_quit(data->main_loop);
        break;
    case GST_MESSAGE_EOS:
        GST_DEBUG("%s End-Of-Stream reached.\n", __func__);
        g_main_loop_quit(data->main_loop);
        break;
    case GST_MESSAGE_STATE_CHANGED:
    {
        GstState old_state, new_state, pending_state;
        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
        GST_DEBUG("%s: state changed from %s to %s\n", GST_MESSAGE_SRC_NAME(msg), gst_element_state_get_name(old_state),
                gst_element_state_get_name(new_state));
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->pipeline))
        {
            GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(data->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, \
            g_strdup_printf("libtranscoding_dot.%s_%s", gst_element_state_get_name(old_state), gst_element_state_get_name(new_state)));
            data->playing = FALSE;
        }
        break;
    }
    default:
        break;
    }

    /* We want to keep receiving messages */
    return TRUE;
}


static void *video_transcoding_workloop(HANDLE *handle) {
    /* chech input param */
    if (NULL == handle) {
        GST_ERROR("%s invalid param!\n", __func__);
        return NULL;
    }

    CustomData *data = (CustomData *)handle;
    /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
    // GstBus *bus = gst_element_get_bus (data->pipeline);
    // gst_bus_add_signal_watch (bus);
    // g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_callback, &data);

    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(data->pipeline));
    gst_bus_add_watch(bus, (GstBusFunc)handle_message, data);
    gst_object_unref(bus);
    g_main_loop_run(data->main_loop);

    GST_DEBUG("%s work loop start work\n", __func__);
    return NULL;
}


int video_transcoding_writeData(HANDLE handle, void* buffer, gint buff_size){
    GstFlowReturn ret;

    /* chech input param */
    if (NULL == handle) {
        GST_ERROR("%s invalid param!\n", __func__);
        return -1;
    }
    CustomData *data = (CustomData *)handle;
    /* push sample to appsrc */
    //g_signal_emit_by_name(data->app_source, "push-sample", &sample);
    /* for not size set */
    //gst_buffer = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, buffer, buff_size, 0 ,buff_size, NULL, NULL);
    GstBuffer *gst_buffer = gst_buffer_copy((GstBuffer *)buffer);
    ret = gst_app_src_push_buffer(data->app_source, gst_buffer);
    if (ret != GST_FLOW_OK) {
        /* We got some error, stop sending data */
        GST_ERROR("%s write data error\n", __func__);
        return -1;
    }
    GST_DEBUG("%s write data addr: %p  wirite data size: %d\n", __func__, buffer, buff_size);
    return 0;
}

static GstFlowReturn appsink_pull_data_callback(GstElement *sink, CustomData *data ){
    GstSample *sample;
    GstBuffer *out_buffer;
    gint out_size;
    //HANDLE *handle = (HANDLE *)data;
    /* chech input param */
    if (NULL == data) {
        GST_ERROR("%s invalid param!\n", __func__);
        return GST_FLOW_ERROR;
    }
    /* Retrieve the buffer */
    g_signal_emit_by_name (sink, "pull-sample", &sample);
    if (NULL != sample) {
        out_buffer = gst_sample_get_buffer(sample);
        out_size = gst_buffer_get_size(out_buffer);
        /* The only thing we do in this example is print a * to indicate a received buffer */
        GST_DEBUG ("%s out_buffer addr: %p  out_buffer size: %d\n", __func__, out_buffer, out_size);
        //video_transcoding_pulldata_callback(handle, buffer, out_size);

        /* app pull data callback */
        data->cus_pull_data_callback((HANDLE *)data, out_buffer, out_size);
        gst_sample_unref (sample);
        return GST_FLOW_OK;
    }

  return GST_FLOW_ERROR;
}

int video_transcoding_setSinkCallback(HANDLE *handle, video_transcoding_callback video_transcoding_pulldata_callback) {
    //GstSample* sample = NULL;
    /* chech input param */
    if (NULL == handle) {
        GST_ERROR("%s invalid param!\n", __func__);
        return -1;
    }
    CustomData *data = (CustomData *)handle;
    /* get sample from appsink */
    //g_signal_emit_by_name (data->app_sink, "pull-sample", &sample);
    data->cus_pull_data_callback = video_transcoding_pulldata_callback;
    //printf("video_transcoding_pulldata_callback address : %p\n", data->cus_pull_data_callback)
    g_signal_connect (data->app_sink, "new-sample", G_CALLBACK (appsink_pull_data_callback), data);
    GST_DEBUG("%s get data done\n", __func__);
    return 0;
}

int video_transcoding_start(HANDLE *handle) {
    GstStateChangeReturn ret;
    /* check input param */
    if (NULL == handle)
    {
        GST_ERROR("%s input param error\n", __func__ );
        return -1;
    }
    CustomData *data = (CustomData *)handle;
    /* start playing  */
    ret = gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        GST_ERROR("%s Unable to set the pipeline to the PLAYING state.\n", __func__);
        gst_object_unref(data->pipeline);
        return -1;
    }
    return 0;
}

int video_transcoding_stop(HANDLE *handle) {
    GstStateChangeReturn ret;
    /* check input param */
    if (NULL == handle)
    {
        GST_ERROR("%s input param error\n", __func__ );
        return -1;
    }
    CustomData *data = (CustomData *)handle;

    ret = gst_element_set_state(data->pipeline, GST_STATE_NULL);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        GST_ERROR("%s Unable to set the pipeline to the NULL state.\n", __func__);
        gst_object_unref(data->pipeline);
        return -1;
    }
    return 0;
}




