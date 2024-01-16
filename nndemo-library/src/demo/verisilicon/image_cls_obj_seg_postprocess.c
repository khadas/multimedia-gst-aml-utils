/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * aml_nnsdk
 */

/*-------------------------------------------
                Includes
-------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "nn_sdk.h"
#include "nn_util.h"
#include "cv_postprocess.h"

// for test performance time
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

/*-------------------------------------------
                  Functions
-------------------------------------------*/

/***************************  image classify top5   **************************************/
void process_top5(float *buf,unsigned int num,img_classify_out_t* clsout)
{
    int j = 0;
    unsigned int MaxClass[5]={0};
    float fMaxProb[5]={0.0};

    float *pfMaxProb = fMaxProb;
    unsigned int  *pMaxClass = MaxClass,i = 0;

    for (j = 0; j < 5; j++)
    {
        for (i=0; i<num; i++)
        {
            if ((i == *(pMaxClass+0)) || (i == *(pMaxClass+1)) || (i == *(pMaxClass+2)) ||
                (i == *(pMaxClass+3)) || (i == *(pMaxClass+4)))
            {
                continue;
            }

            if (buf[i] > *(pfMaxProb+j))
            {
                *(pfMaxProb+j) = buf[i];
                *(pMaxClass+j) = i;
            }
        }
    }
    for (i=0; i<5; i++)
    {
        if (clsout == NULL)
        {
            LOGI("%3d: %8.6f\n", MaxClass[i], fMaxProb[i]);
        }
        else
        {
            clsout->score[i] = fMaxProb[i];
            clsout->topClass[i] = MaxClass[i];
        }
    }
}
/***************************  image classify top5   **************************************/

/***************************  IMAGE_CLASSIFY postprocess **************************************/
void* postprocess_classify(nn_output *pout)
{
    static img_classify_out_t cls_result;
    memset(&cls_result,0,sizeof(img_classify_out_t));
    // if (pout->out[0].param->data_format == NN_BUFFER_FORMAT_FP32)
    // {
    //     process_top5((float*)pout->out[0].buf,pout->out[0].size/sizeof(float),&cls_result);
    // }
    process_top5((float*)pout->out[0].buf,pout->out[0].size/sizeof(float),&cls_result);

    return (void*)&cls_result;
}
/***************************  IMAGE_CLASSIFY postprocess **************************************/

void* postprocess_yolov2(nn_output *pout)
{
    float* obj_out;
    int output_num;

    obj_out = (float*)pout->out[0].buf;
    output_num = pout->out[0].size/sizeof(float);

    return object_detect_postprocess(obj_out,416,416,13,13,output_num);
}

//post_process in AML_PERF_OUTPUT_GET, now we move to here
void data_to_fp32(float *new_buffer, unsigned char *old_buffer, int sz, float scale, int zero_point, int type)
{
    if (NN_BUFFER_FORMAT_UINT8 == type)
    {
        for (int i = 0; i < sz; i++)
        {
            new_buffer[i] = (float)(((uint8_t*)old_buffer)[i] - zero_point) * scale;
        }
    }
    else if (NN_BUFFER_FORMAT_INT8 == type)
    {
        for (int i = 0; i < sz; i++)
        {
            new_buffer[i] = (float)(((int8_t*)old_buffer)[i] - zero_point) * scale;
        }
    }
}


void* postprocess_yolov3(nn_output *pout)
{
#if 1
    // support multiple thread
    // support INT8 input
    LOGI("wo select multi thread path\n");

    // unsigned char *yolov3_buffer[3] = {NULL};

    // yolov3_buffer[0] = pout->out[2].buf;
    // yolov3_buffer[1] = pout->out[1].buf;
    // yolov3_buffer[2] = pout->out[0].buf;

    return yolov3_postprocess_multi_thread(pout);
#else
    printf("wo select single thread path\n");

    void* out = NULL;
    float *yolov3_buffer[3] = {NULL};
    float *fp32_buffer[3] = {NULL};
    int size  = 0;
    float scale = 0.0;
    int zp = 0;
    int data_format;

    aml_platform_info_t platform_info;
    memset(&platform_info, 0, sizeof(platform_info));
    aml_read_chip_info(&platform_info);

    for (int i = 0; i < pout->num; i++)
    {
        size = pout->out[i].size;
        data_format = pout->out[i].param->data_format;
        scale = pout->out[i].param->quant_data.affine.scale;
        zp = pout->out[i].param->quant_data.affine.zeroPoint;
        LOGI("size: %d, data_format: %d, scale: %f, zp: %d\n", size, data_format, scale, zp);

        struct timeval start;
        struct timeval end;
        double time_total;
        gettimeofday(&start, NULL);

        fp32_buffer[i] = (float *)malloc(size * sizeof(float));//free
        data_to_fp32(fp32_buffer[i], pout->out[i].buf, size, scale, zp, data_format);


        gettimeofday(&end, NULL);
        time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
        start = end;
        LOGI("data_to_fp32, i=%d time=%lf uS \n", i, time_total);
    }

    // if (platform_info.hw_type == AML_HARDWARE_VSI_UNIFY)
    // {
    //     yolov3_buffer[0] = fp32_buffer[2];
    //     yolov3_buffer[1] = fp32_buffer[1];
    //     yolov3_buffer[2] = fp32_buffer[0];
    // }
    // else if (platform_info.hw_type == AML_HARDWARE_ADLA)
    // {
    //     yolov3_buffer[0] = fp32_buffer[0];
    //     yolov3_buffer[1] = fp32_buffer[1];
    //     yolov3_buffer[2] = fp32_buffer[2];
    // }

    yolov3_buffer[0] = fp32_buffer[2];
    yolov3_buffer[1] = fp32_buffer[1];
    yolov3_buffer[2] = fp32_buffer[0];

    out = yolov3_postprocess(yolov3_buffer,416,416,13,13,0);

    for (int i = 0; i < pout->num; i++)
    {
        if (fp32_buffer[i])
        {
            free(fp32_buffer[i]);
            fp32_buffer[i] = NULL;
        }
    }
    return out;
#endif
}



void* postprocess_object_detect(nn_output *pout)
{
    float* obj_out;
    int output_num;

    obj_out = (float*)pout->out[0].buf;
    output_num = pout->out[0].size/sizeof(float);

    return object_detect_postprocess(obj_out,416,416,13,13,output_num);
}


void* postprocess_segmentation(nn_output *pout)
{
    float *buffer;
    int i,j,m,n;
    unsigned int sz;
    static segment_out_t *segment_result = NULL;

    static float result_buffer[127][255][19];
    float max;
    int flag = 0;
    float sum;

    segment_result = (segment_out_t*)malloc(sizeof(segment_out_t));

    memset(segment_result, 0, sizeof(segment_out_t));
    segment_result->segOut.data = (unsigned char *)malloc(sizeof(char)*127*255*3);


    buffer = (float*)pout->out[0].buf;
    sz= pout->out[0].size;
    memcpy(result_buffer,buffer,sz);
    /********************** argmax and one-hot **************************/
    for (i = 0;i < 127;i++)
    {
        for (j = 0;j < 255;j++)
        {
            max = result_buffer[i][j][0];
            for (m = 0;m < 19;m++)
            {
                if (max < result_buffer[i][j][m])
                    max = result_buffer[i][j][m];
            }

            for (m = 0; m < 19;m++)
            {
                if (max > result_buffer[i][j][m])
                    result_buffer[i][j][m] = 0;
                else if (flag == 0)
                {
                    result_buffer[i][j][m] = 1;
                    flag = 1;
                } else
                    result_buffer[i][j][m] = 0;
            }
            flag = 0;
        }
    }
    /********************** matmul **************************/
    char label[19][3] = {{128, 64, 128}, {244, 35, 231}, {69, 69, 69}
                            ,{102, 102, 156}, {190, 153, 153}, {153, 153, 153}
                            ,{250, 170, 29}, {219, 219, 0}, {106, 142, 35},{152, 250, 152}, {69, 129, 180}, {219, 19, 60}
                            ,{255, 0, 0}, {0, 0, 142}, {0, 0, 69},{0, 60, 100}, {0, 79, 100}, {0, 0, 230},{119, 10, 32}};
    unsigned char output[127][255][3];
    for (i = 0; i < 127;i++)
    {
        for (j =0;j <255;j++ )
        {
            for (m = 0; m < 3;m++)
            {
                sum = 0;
                for (n = 0;n < 19;n++)
                {
                    sum = sum + result_buffer[i][j][n] * label[n][m];
                }
                output[i][j][m] = (unsigned char)sum;
            }
        }
    }
    memcpy(segment_result->segOut.data,output,127*255*3);
    segment_result->segOut.height = 127;
    segment_result->segOut.width = 255;
    segment_result->segOut.channel = 3;
    return (void*)segment_result;
}