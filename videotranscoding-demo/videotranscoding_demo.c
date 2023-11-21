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
#include <string.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/gstbuffer.h>
#include "video_transcoding.h"


CodecType tset_code = MPEG4;

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData_App {
    GstElement *pipeline;
    GstElement *source;
    GstElement *demuxer;
    GstElement *video_queue;
    GstElement *video_queue1;
    GstElement *audio_queue;
    GstElement *videoparser;
    GstElement *videoparser1;
    GstElement *sink;
    GstElement *audioparser;
    GstElement *muxer;
    GstElement *filesink;
    GstElement *app_source;

} CustomData_App;

// init the global parmam
video_transcoding_param global_video_transcoding_param;

/* library handle */
HANDLE *handle = NULL;
/* app handle */
HANDLE *handle_app = NULL;


CodecType mapStringToCodec(const char *codec) {
    if (strcmp(codec, "AV1") == 0) {
        return AV1;
    } else if (strcmp(codec, "AVS2") == 0) {
        return AVS2;
    } else if (strcmp(codec, "AVS3") == 0) {
        return AVS3;
    } else if (strcmp(codec, "AVS") == 0) {
        return AVS;
    } else if (strcmp(codec, "H264") == 0) {
        return H264;
    } else if (strcmp(codec, "H265") == 0) {
        return H265;
    } else if (strcmp(codec, "JPEG") == 0) {
        return JPEG;
    } else if (strcmp(codec, "MPEG4") == 0) {
        return MPEG4;
    } else if (strcmp(codec, "VC1") == 0) {
        return VC1;
    } else if (strcmp(codec, "VP9") == 0) {
        return VP9;
    }

    g_printerr("%s no match input codec %s\n", __func__, codec);
    return INVALID_CODEC;
}


void parseConfigFile(const char *filename, video_transcoding_param *param){
    //char codecStr[20];
    char src_codec_value[20];
    char dst_codec_value[20];
    FILE *file = fopen(filename,"r");
    if (file == NULL) {
        g_printerr("Error opening config file %s\n", filename);
        param->dst_size.width = 640;
        param->dst_size.height = 480;
        param->dst_codec = H264;
        param->dst_framerate = 30;
        param->bitrate_kb = 3000;
        param->gop_size = 60;
        g_print("%s set the default value\n dst_width=%d dst_height=%d dst_codec=%d dst_framerate=%d/1\n \
                bitrate_kb=%d gop_size=%d\n", __func__, param->dst_size.width,param->dst_size.height,param->dst_codec,
                param->dst_framerate,param->bitrate_kb,param->gop_size);
        return;
    }
    /* parse the config file */
    fscanf(file, "src_size={%d, %d}\n", &param->src_size.width, &param->src_size.height);
    //fscanf(file, "src_codec=%d\n", &param->src_codec);
    fscanf(file, "src_codec=%s\n", src_codec_value);
    param->src_codec = mapStringToCodec(src_codec_value);
    fscanf(file, "src_framerate=%d\n,", &param->src_framerate);
    fscanf(file, "dst_size={%d, %d}\n", &param->dst_size.width, &param->dst_size.height);
    fscanf(file, "dst_codec=%s\n", dst_codec_value);
    param->dst_codec = mapStringToCodec(dst_codec_value);
    fscanf(file, "dst_framerate=%d\n", &param->dst_framerate);
    fscanf(file, "bitrate_kb=%d\n", &param->bitrate_kb);
    fscanf(file, "gop_size=%d\n", &param->gop_size);

    fclose(file);
}

static void SignalHandler(int signum){
    if ( NULL == handle ) {
        return;
    }
    video_transcoding_stop(handle);
    video_transcoding_deinit(handle);
    exit(1);
}

static GstFlowReturn new_sample(GstElement *appsink, gpointer data) {
    GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
    if (sample) {
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        // GstCaps *caps = gst_sample_get_caps(sample);
        // gchar *caps_str = gst_caps_to_string(caps);
        // g_print("Caps: %s\n", caps_str);
        //g_free(caps_str);

        if (buffer) {
            gsize size = gst_buffer_get_size(buffer);
            g_print("Received ES data: %d, %s\n", (int)size, (const gchar *)data);
            // save the out data
            video_transcoding_writeData(handle, buffer, size);
            gst_sample_unref(sample);
            return GST_FLOW_OK;
        }

    }

    return GST_FLOW_ERROR;
}

/* This function will be called by the pad-added signal */
static void pad_added_handler (GstElement *src, GstPad *new_pad, CustomData_App *data) {
  GstPad *sink_pad = NULL;
  GstPadLinkReturn ret;
  GstCaps *new_pad_caps = NULL;
  GstStructure *new_pad_struct = NULL;
  const gchar *new_pad_type = NULL;

  g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

  // chech the pad is the source pad
  if (GST_PAD_DIRECTION(new_pad) != GST_PAD_SRC)
      return;

  /* Check the new pad's type */
  new_pad_caps = gst_pad_get_current_caps (new_pad);
  new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
  new_pad_type = gst_structure_get_name (new_pad_struct);


  // determine audio or video based on caps
  if (g_strrstr(new_pad_type, "video")) {
      g_print("New pad is a video pad.\n");
      sink_pad = gst_element_get_static_pad (data->video_queue, "sink");
      // connect to video processing flow
  } else if (g_strrstr(new_pad_type, "audio")) {
      sink_pad = gst_element_get_static_pad (data->audio_queue, "sink");
      // connect to audio processing flow
  } else {
      g_print("%s New pad is of unknown type.\n",new_pad_type);
      return;
  }

  /* If our converter is already linked, we have nothing to do here */
  if (gst_pad_is_linked (sink_pad)) {
    g_print ("We are already linked. Ignoring.\n");
    goto exit;
  }

  /* Attempt the link */
  ret = gst_pad_link (new_pad, sink_pad);
  if (GST_PAD_LINK_FAILED (ret)) {
    g_print ("Type is '%s' but link failed.\n", new_pad_type);
  } else {
    g_print ("Link succeeded (type '%s').\n", new_pad_type);
  }

exit:
  /* Unreference the new pad's caps, if we got them */
  if (new_pad_caps != NULL)
    gst_caps_unref (new_pad_caps);

  /* Unreference the sink pad */
  gst_object_unref (sink_pad);
}

int video_transcoding_pulldata_callback(void *buffer, gint size){
  CustomData_App *data = (CustomData_App *)handle_app;
  GstStateChangeReturn ret=GST_FLOW_OK;
  if (NULL != buffer) {
    /* The only thing we do in this example is print a * to indicate a received buffer */
    g_print ("%s : ****** \n", __func__);
    GstBuffer *gst_buffer = gst_buffer_copy((GstBuffer *)buffer);
    ret = gst_app_src_push_buffer(data->app_source, gst_buffer);
    if (ret != GST_FLOW_OK) {
        /* We got some error, stop sending data */
        printf("write data error\n");
        return -1;
    }
    printf("%s : write data func done\n", __func__);
    return 0;
  }
  gst_element_send_event(data->app_source, gst_event_new_eos());
  return 0;
}

void send_eos_event (GstElement* appsink, HANDLE handle){
    printf("%s we send eos enent to lib\n",__func__);
    CustomData *data = (CustomData *)handle;
    gst_element_send_event(data->app_source, gst_event_new_eos());
    return;
}



int main(int argc, char *argv[]) {
    CustomData_App *data = (CustomData_App *)malloc(sizeof(CustomData_App));
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    GstCaps* src_caps = NULL;
    const char *mediaFile = argv[1];
    const char *configFile = argv[2];

    /* check input param */
    if (argc !=3 ) {
        g_printerr("Usage: %s <media_file> <config_file>\n", argv[0]);
        return -1;
    }

    signal(SIGINT,SignalHandler);

    /* read param from config file */
    parseConfigFile(configFile, &global_video_transcoding_param);

    /* init the gstreamer library */
    gst_init(&argc, &argv);

    data->pipeline = gst_pipeline_new("demo-pipeline");

    data->source = gst_element_factory_make("filesrc", "demo-file-source");
    g_object_set(G_OBJECT(data->source), "location", argv[1], NULL);

    data->demuxer = gst_element_factory_make("tsdemux", "demo-ts-demuxer");

    data->videoparser = gst_element_factory_make("mpegvideoparse", "demo-mpegvideo-parser");

    //data->videoparser1 = gst_element_factory_make("h264parse", "h264parse-parser");

    data->video_queue = gst_element_factory_make ("queue", "demo-video_queue");

    data->audio_queue = gst_element_factory_make ("queue", "demo-audio_queue");

    data->sink = gst_element_factory_make("appsink", "demo-my-sink");

    data->audioparser = gst_element_factory_make("ac3parse", "demo-ac3audio-parser");

    data->muxer = gst_element_factory_make("mpegtsmux", "demo-ts-muxer");

    data->filesink = gst_element_factory_make("filesink", "demo-my-filesink");
    g_object_set(G_OBJECT(data->filesink), "location", "/data/test.ts", NULL);
    data->app_source = gst_element_factory_make ("appsrc", "demo-app_source_test");

    switch (global_video_transcoding_param.dst_codec)
    {
    case H264:
        data->videoparser1 = gst_element_factory_make("h264parse", "demo-h264parse-parser");
        src_caps = gst_caps_new_simple("video/x-h264",
                                    "stream-format", G_TYPE_STRING, "byte-stream",
                                        "alignment",G_TYPE_STRING,"au",
                                        //"width", G_TYPE_INT, 720,
                                        //"height", G_TYPE_INT, 420,
                                        // "framerate", GST_TYPE_FRACTION, param->src_framerate,
                                        NULL);
        break;

    case H265:
        data->videoparser1 = gst_element_factory_make("h265parse", "demo-h265parse-parser");
        src_caps = gst_caps_new_simple("video/x-h265",
                                        "stream-format", G_TYPE_STRING, "byte-stream",
                                        "alignment",G_TYPE_STRING,"nal",
                                        //"width", G_TYPE_INT, 720,
                                        //"height", G_TYPE_INT, 420,
                                        // "framerate", GST_TYPE_FRACTION, param->src_framerate,
                                        NULL);
        break;
    default:
        break;
    }

        /* configure the appsrc */
     g_object_set (data->app_source, "caps", src_caps, NULL);

    /* Check if all elements were successfully created */
    if (!data->pipeline || !data->source || !data->demuxer || !data->videoparser || !data->videoparser1
        || !data->sink || !data->audioparser || !data->muxer || !data->video_queue || !data->audio_queue || !data->app_source) {
        g_print("One or more elements could not be created. Exiting.\n");
        return -1;
    }

    /* Add elements to the pipeline */
    gst_bin_add(GST_BIN(data->pipeline), data->source);
    gst_bin_add(GST_BIN(data->pipeline), data->demuxer);
    gst_bin_add(GST_BIN(data->pipeline), data->video_queue);
    gst_bin_add(GST_BIN(data->pipeline), data->audio_queue);
    gst_bin_add(GST_BIN(data->pipeline), data->videoparser);
    gst_bin_add(GST_BIN(data->pipeline), data->sink);
    gst_bin_add(GST_BIN(data->pipeline), data->audioparser);
    gst_bin_add(GST_BIN(data->pipeline), data->app_source);
    gst_bin_add(GST_BIN(data->pipeline), data->videoparser1);
    gst_bin_add(GST_BIN(data->pipeline), data->muxer);
    gst_bin_add(GST_BIN(data->pipeline), data->filesink);

    //|| !gst_element_link_many(data->app_source, data->muxer, data->filesink, NULL)
    // || !gst_element_link(data->muxer, data->filesink)

    /* Link elements */
    if (!gst_element_link(data->source, data->demuxer) || !gst_element_link_many(data->video_queue, data->videoparser, data->sink, NULL)
        || !gst_element_link_many(data->app_source, data->videoparser1, data->muxer, data->filesink, NULL)
        || !gst_element_link_many(data->audio_queue, data->audioparser, data->muxer, NULL)) {
        g_print("Elements could not be linked. Exiting.\n");
        return -1;
    }

    /* Set appsink */
    g_object_set(G_OBJECT(data->sink), "emit-signals", TRUE, "sync", FALSE, NULL);
    g_signal_connect(G_OBJECT(data->sink), "new-sample", G_CALLBACK(new_sample), NULL);

    /* Connect the "pad-added" signal handler */
    g_signal_connect(data->demuxer, "pad-added", G_CALLBACK(pad_added_handler), data);

    handle_app=(HANDLE)data;
    handle = video_transcoding_init(&global_video_transcoding_param, argc, NULL);
    int ret_lib=video_transcoding_start(handle);
    if (ret_lib == -1) {
        g_printerr("lib pip: Unable to set the pipeline to the playing state.\n");
        gst_object_unref(data->pipeline);
        return -1;
    }
    /* send eos event */
    g_signal_connect(G_OBJECT(data->sink), "eos", G_CALLBACK(send_eos_event), handle);


    /* Set pipeline status to playing */
    ret = gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(data->pipeline);
        return -1;
    }

    video_transcoding_callback pull_data_callback = video_transcoding_pulldata_callback;
    video_transcoding_setSinkCallback(handle, pull_data_callback);

    /* main loop */
    bus = gst_element_get_bus(data->pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_EOS);
    //msg = gst_bus_poll(bus, GST_MESSAGE_EOS, GST_CLOCK_TIME_NONE);
    //gst_message_unref(msg);

    if (msg != NULL) {
        printf("release the source demo to do\n");
        video_transcoding_stop(handle);
        video_transcoding_deinit(handle);
    }

    /* release source */
    gst_element_set_state(data->pipeline, GST_STATE_NULL);
    gst_object_unref(data->pipeline);

    return 0;
}

