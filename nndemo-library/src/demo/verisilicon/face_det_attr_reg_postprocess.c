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
#define LOG_TAG "face_det_attr_reg_postprocess"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "nn_sdk.h"
#include "nn_util.h"
#include "cv_postprocess.h"
#include "postprocess_util.h"


/*******************     face attribute postprocess      ********************/
/****************************************************************************/
void* postprocess_gender(nn_output *pout)
{
    float* buffer;
    static face_gender_out_t gender_result;
    if (pout->out[0].param->data_format == NN_BUFFER_FORMAT_FP32)
    {
        buffer = (float*)pout->out[0].buf;
        //printf("buffer[0]:%f,buffer[1]:%f\n",buffer[0],buffer[1]);
        gender_result.gender = buffer[0];
    }
    return (void*)&gender_result;
}

void* postprocess_age(nn_output *pout)
{
    float* buffer;
    unsigned int sz ;
    static face_age_out_t age_result;
    float age;
    float pred_age_s1[3];
    float pred_age_s2[3];
    float pred_age_s3[3];

    float local_s1[3];
    float local_s2[3];
    float local_s3[3];

    float delta_s1;
    float delta_s2;
    float delta_s3;

    unsigned int i,S1,S2,S3,lambda_local,lambda_d;
    float a,b,c;

    for (i=0;i<pout->num;i++)
    {
        buffer = (float *)pout->out[i].buf;
        sz= pout->out[i].size;
        switch (i)
        {
            case 0:
                memcpy(pred_age_s1,buffer,sz);
                break;
            case 1:
                memcpy(pred_age_s2,buffer,sz);
                break;
            case 2:
                memcpy(pred_age_s3,buffer,sz);
                break;
            case 3:
                memcpy(local_s1,buffer,sz);
                break;
            case 4:
                memcpy(local_s2,buffer,sz);
                break;
            case 5:
                memcpy(local_s3,buffer,sz);
                break;
            case 6:
                memcpy(&delta_s1,buffer,sz);
                break;
            case 7:
                memcpy(&delta_s2,buffer,sz);
                break;
            case 8:
                memcpy(&delta_s3,buffer,sz);
                break;
            default:
                break;
        }
    }
    S1 = 3;
    S2 = 3;
    S3 = 3;
    lambda_local = 1;
    lambda_d = 1;
    a = 0;
    b = 0;
    c = 0;

    for (i = 0;i < 3;i++)
    {
        a = a + (i + lambda_local * local_s1[i]) * pred_age_s1[i];
    }
    a = a /(S1*(1 + lambda_d * delta_s1));
    for (i = 0;i < S2;i++)
    {
        b = b + (i + lambda_local * local_s2[i]) * pred_age_s2[i];
    }
    b = b /(S1*(1 + lambda_d * delta_s1)) / (S2*(1+lambda_d*delta_s2)) ;

    for (i = 0;i < S3;i++)
    {
        c = c + (i + lambda_local * local_s3[i]) * pred_age_s3[i];
    }
    c = c /(S1*(1 + lambda_d * delta_s1)) / (S2*(1+lambda_d*delta_s2)) / (S3*(1+lambda_d*delta_s3));

    age = (a+b+c)*101;
    age_result.age = (int)age;

    return (void*)&age_result;
}


void* postprocess_emotion(nn_output *pout)
{
    float *buffer;
    int j = 0;
    unsigned int sz,i;

    unsigned int MaxClass[5]={0};
    float fMaxProb[5]={0.0};
    float *pfMaxProb = fMaxProb;
    unsigned int  *pMaxClass = MaxClass;
    static face_emotion_out_t *emotion_result = NULL;

    emotion_result = (face_emotion_out_t*)malloc(sizeof(face_emotion_out_t));
    memset(emotion_result, 0, sizeof(face_emotion_out_t));

    buffer = (float*)pout->out[0].buf;
    sz= pout->out[0].size;

    for (j = 0; j < 5; j++)
    {
        for (i=0; i<sz/4; i++)
        {
            if ((i == *(pMaxClass+0)) || (i == *(pMaxClass+1)) || (i == *(pMaxClass+2)) ||
                (i == *(pMaxClass+3)) || (i == *(pMaxClass+4)))
            {
                continue;
            }

            if (buffer[i] > *(pfMaxProb+j))
            {
                *(pfMaxProb+j) = buffer[i];
                *(pMaxClass+j) = i;
            }
        }
    }
    emotion_result->emotion= MaxClass[0];
    emotion_result->prob= fMaxProb[0];
    return (void*)emotion_result;
}

/****************************************************************************/
/*******************     face attribute postprocess      ********************/


/*******************     face detect postprocess      ********************/
/****************************************************************************/
void* postprocess_facedet(nn_output *nn_out)
{
    unsigned int i = 0;
    int8_t *p = NULL;
    unsigned int buf_size = 0;
    nn_output *nn_out_tmp = NULL;  // float
	nn_output *pout = NULL;

	// read chip info
    aml_platform_info_t platform_info;
    memset(&platform_info, 0, sizeof(platform_info));
    aml_read_chip_info(&platform_info);

    if (platform_info.hw_type == AML_HARDWARE_VSI_UNIFY)
    {
        // needn't convert
        pout = nn_out;
    }
    else if (platform_info.hw_type == AML_HARDWARE_ADLA)
    {
        // convert INT8 to float32
        buf_size = sizeof(nn_output);
        for (i = 0; i < nn_out->num; i++)
        {
            buf_size += sizeof(nn_buffer_params_t);
            buf_size += nn_out->out[i].size*sizeof(float);
        }
        nn_out_tmp = (nn_output *)malloc(buf_size);
        // copy nn_out to nn_out_tmp
        memcpy(nn_out_tmp, nn_out, sizeof(nn_output));

        p = (int8_t *)nn_out_tmp;
        p += sizeof(nn_output);
        for (i = 0; i < nn_out->num; i++)
        {
            nn_out_tmp->out[i].param = (nn_buffer_params_t *)p;
            memcpy(nn_out_tmp->out[i].param, nn_out->out[i].param, sizeof(nn_buffer_params_t));
            p += (sizeof(nn_buffer_params_t));

            int float_size = nn_out->out[i].size*sizeof(float);
            nn_out_tmp->out[i].buf = (unsigned char *)p;
            p += float_size;
            nn_out_tmp->out[i].size = float_size;
            nn_out_tmp->out[i].out_format = AML_OUTDATA_FLOAT32;
        }

        pp_dtypeToF32(nn_out_tmp, nn_out);
        pout = nn_out_tmp;
    }

    // memcpy bbox, prob
    float *buffer;
    unsigned int sz ;
    // unsigned int i;
    static face_detect_out_t face_det_result;
    memset(&face_det_result,0,sizeof(face_detect_out_t));
    for (i=0;i<pout->num;i++)
    {
        buffer = (float *)pout->out[i].buf;
        sz= pout->out[i].size;
        switch (i)
        {
            case 0:
                memcpy(prob_32,buffer,sz);
                break;
            case 1:
                memcpy(bbox_32,buffer,sz);
                break;
            case 2:
                memcpy(land_32,buffer,sz);
                break;
            case 3:
                memcpy(prob_16,buffer,sz);
                break;
            case 4:
                memcpy(bbox_16,buffer,sz);
                break;
            case 5:
                memcpy(land_16,buffer,sz);
                break;
            case 6:
                memcpy(prob_8,buffer,sz);
                break;
            case 7:
                memcpy(bbox_8,buffer,sz);
                break;
            case 8:
                memcpy(land_8,buffer,sz);
                break;
            default:
                break;
        }
    }

    face_detect_postprocess(&face_det_result);

	if (NULL != nn_out_tmp)
	{
		free(nn_out_tmp);
		nn_out_tmp = NULL;
	}

    return (void*)(&face_det_result);
}



void* postprocess_yoloface_v2(nn_output *pout)
{
    float* obj_out;
    int output_num;

    obj_out = (float*)pout->out[0].buf;
    output_num = pout->out[0].size/sizeof(float);
    return yoloface_detect_postprocess(obj_out,416,416,13,13,output_num);
}

void* postprocess_faceland5(nn_output *nn_out)
{
    unsigned int i = 0;
    int8_t *p = NULL;
    unsigned int buf_size = 0;
    nn_output *nn_out_tmp = NULL;  // float
	nn_output *pout = NULL;

	// read chip info
    aml_platform_info_t platform_info;
    memset(&platform_info, 0, sizeof(platform_info));
    aml_read_chip_info(&platform_info);

    if (platform_info.hw_type == AML_HARDWARE_VSI_UNIFY)
    {
        // needn't convert
        pout = nn_out;
    }
    else if (platform_info.hw_type == AML_HARDWARE_ADLA)
    {
        // convert INT8 to float32
        buf_size = sizeof(nn_output);
        for (i = 0; i < nn_out->num; i++)
        {
            buf_size += sizeof(nn_buffer_params_t);
            buf_size += nn_out->out[i].size*sizeof(float);
        }
        nn_out_tmp = (nn_output *)malloc(buf_size);
        // copy nn_out to nn_out_tmp
        memcpy(nn_out_tmp, nn_out, sizeof(nn_output));

        p = (int8_t *)nn_out_tmp;
        p += sizeof(nn_output);
        for (i = 0; i < nn_out->num; i++)
        {
            nn_out_tmp->out[i].param = (nn_buffer_params_t *)p;
            memcpy(nn_out_tmp->out[i].param, nn_out->out[i].param, sizeof(nn_buffer_params_t));
            p += (sizeof(nn_buffer_params_t));

            int float_size = nn_out->out[i].size*sizeof(float);
            nn_out_tmp->out[i].buf = (unsigned char *)p;
            p += float_size;
            nn_out_tmp->out[i].size = float_size;
            nn_out_tmp->out[i].out_format = AML_OUTDATA_FLOAT32;
        }

        pp_dtypeToF32(nn_out_tmp, nn_out);
        pout = nn_out_tmp;
    }

    float *buffer;
    unsigned int sz ;
    // unsigned int i;
    static face_landmark5_out_t face_landmark5_result;
    memset(&face_landmark5_result,0,sizeof(point_t));
    for (i=0;i<pout->num;i++)
    {
        buffer = (float *)pout->out[i].buf;
        sz= pout->out[i].size;
        switch (i)
        {
            case 0:
                memcpy(prob_32,buffer,sz);
                break;
            case 1:
                memcpy(bbox_32,buffer,sz);
                break;
            case 2:
                memcpy(land_32,buffer,sz);
                break;
            case 3:
                memcpy(prob_16,buffer,sz);
                break;
            case 4:
                memcpy(bbox_16,buffer,sz);
                break;
            case 5:
                memcpy(land_16,buffer,sz);
                break;
            case 6:
                memcpy(prob_8,buffer,sz);
                break;
            case 7:
                memcpy(bbox_8,buffer,sz);
                break;
            case 8:
                memcpy(land_8,buffer,sz);
                break;
            default:
                break;
        }
    }
    face_landmark5_postprocess(&face_landmark5_result);

	if (NULL != nn_out_tmp)
	{
		free(nn_out_tmp);
		nn_out_tmp = NULL;
	}

    return (void*)(&face_landmark5_result);
}

void* postprocess_faceland68(nn_output *pout)
{
    float *buffer;
    unsigned int i;
    int j;

    static face_landmark68_out_t face_landmark68_result;
    memset(&face_landmark68_result, 0, sizeof(face_landmark68_out_t));
    face_landmark68_result.detNum = pout->num;

    for (i=0;i<pout->num;i++)
    {
        buffer = (float *)pout->out[i].buf;
        for (j=0; j< 68; j++)
        {
            face_landmark68_result.pos[i][j].x = buffer[2 * j] * 60;
            face_landmark68_result.pos[i][j].y = buffer[2 * j + 1 ] * 60;
        }
    }

    return (void*)(&face_landmark68_result);
}

void* postprocess_rfb_facedet(nn_output *pout)
{
    float *buffer;
    unsigned int sz ;
    unsigned int i;
    static face_rfb_detect_out_t face_rfb_det_result;
    memset(&face_rfb_det_result,0,sizeof(face_rfb_detect_out_t));
    for (i=0;i<pout->num;i++)
    {
        buffer = (float *)pout->out[i].buf;
        sz= pout->out[i].size;
        switch (i)
        {
            case 0:
                memcpy(bbox,buffer,sz);
                break;
            case 1:
                memcpy(pprob,buffer,sz);
                break;
            case 2:
                memcpy(llandmark,buffer,sz);
                break;
            default:
                break;
        }
    }
    face_rfb_detect_postprocess(&face_rfb_det_result);
    return (void*)(&face_rfb_det_result);
}

/****************************************************************************/
/*******************     face detect postprocess      ********************/




/*******************     face recognize postprocess      ********************/
/****************************************************************************/
void* postprocess_facenet(nn_output *pout)
{
    unsigned char *buffer = NULL;
    int i;
    static facenet_out_t facenet_result;
    memset(&facenet_result,0,sizeof(facenet_out_t));
    buffer = (unsigned char*)pout->out[0].buf;
    for (i=0;i<128;i++)
    {
        facenet_result.faceVector[i] = buffer[i];
    }
    return (void*)(&facenet_result);
}


void* postprocess_facereg_uint(nn_output *pout)
{
    unsigned char *buffer = NULL;
    int i;
    static face_recognize_uint_out_t face_recognize_uint_result;
    memset(&face_recognize_uint_result,0,sizeof(face_recognize_uint_out_t));
    buffer = (unsigned char*)pout->out[0].buf;

    for (i=0;i<512;i++)
    {
        face_recognize_uint_result.faceVector[i] = buffer[i];
    }

    return (void*)(&face_recognize_uint_result);
}


void* postprocess_facereg(nn_output *pout)
{
    float *buffer;
    int i;
    static face_recognize_out_t face_recognize_result;
    memset(&face_recognize_result, 0, sizeof(face_recognize_out_t));
    if (pout->out[0].param->data_format == NN_BUFFER_FORMAT_FP32)
    {
        buffer = (float*)pout->out[0].buf;
        for (i=0;i<512;i++)
        {
            face_recognize_result.faceVector[i] = buffer[i];
        }
    }
    return (void*)(&face_recognize_result);
}


void* postprocess_facecompare(nn_output *pout)
{
    float *buffer;
    float sum=0, temp=0;
    int i;
    static face_compare_out_t face_compare_result;
    memset(&face_compare_result, 0, sizeof(face_compare_out_t));
    buffer = (float*)pout->out[0].buf;

    if (pout->out[0].param->data_format == NN_BUFFER_FORMAT_FP32)
    {
        for (i=0; i< 128; i++)
        {
            temp = buffer[i] - buffer[i + 128];
            sum += temp * temp;
        }
        temp = sqrt(sum);
    }

    face_compare_result.compareScore = temp;
    return (void*)(&face_compare_result);
}

/****************************************************************************/
/*******************     face recognize postprocess      ********************/