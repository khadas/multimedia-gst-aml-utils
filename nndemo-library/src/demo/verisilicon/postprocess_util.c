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
#include <math.h>
#include "nn_sdk.h"
#include "nn_util.h"
#include "cv_postprocess.h"
#include "postprocess_util.h"



// for test performance time
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>


static struct timeval g_start = {0};
static struct timeval g_end = {0};
static double g_timecost = 0.0f;





//////////////////////////////////////////////////////////////////////////////////////////////
// move normalization to post process
// convert int8 data to float32
void pp_dtypeToF32(nn_output *out_data, nn_output *in_data)
{
    unsigned int i = 0;
    int j = 0;
    int sz = 0;
    int type = 0;
    int zero_point = 0;
    float scale = 0.;

    void *old_buffer = NULL;
    float *new_buffer = NULL;

    for (i = 0; i < in_data->num; i++)
    {
        sz = in_data->out[i].size;//1001
        type = in_data->out[i].param->data_format;
        zero_point = in_data->out[i].param->quant_data.affine.zeroPoint;
        scale = in_data->out[i].param->quant_data.affine.scale;

        old_buffer = in_data->out[i].buf;
        new_buffer = (float *)out_data->out[i].buf;
        for (j = 0; j < sz; j++)
        {
            if (NN_BUFFER_FORMAT_UINT8 == type)
            {
                new_buffer[j] = (float)(((uint8_t*)old_buffer)[j] - zero_point) * scale;
            }
            else if (NN_BUFFER_FORMAT_INT8 == type)
            {
                new_buffer[j] = (float)(((int8_t*)old_buffer)[j] - zero_point) * scale;
            }
        }
    }
}

void pp_nhwc_to_nchw(nn_output *output_data)
{
    unsigned int i = 0;
    int j = 0;
    int k = 0;
    int row = 0;
    int col = 0;
    int cn = 0;
    unsigned char *old_buffer = NULL;
    int8_t *new_buffer = NULL;

    for (i = 0; i < output_data->num; i++)
    {
        old_buffer = output_data->out[i].buf;
        row = output_data->out[i].param->sizes[1];
        col = output_data->out[i].param->sizes[2];
        cn = output_data->out[i].param->sizes[3];
        if (1 == cn)
        {
            continue;
        }

        new_buffer = (int8_t *)malloc(sizeof(int8_t)*row*col*cn);
        memset(new_buffer, 0, sizeof(int8_t)*row*col*cn);
        for (j = 0; j < cn; j++)
        {
            for (k = 0; k < row*col; k++)
            {
                *(new_buffer + k + j*row*col) = *(old_buffer + k*cn + j);
            }
        }

        free(output_data->out[i].buf);
        output_data->out[i].buf = (unsigned char *)new_buffer;//这种写法相当于把buf替换了
    }
}

void pp_nchw_f32(nn_output *out_data, nn_output *in_data)
{
    unsigned int i = 0;
    int j = 0;
    int k = 0;
    int row = 0;
    int col = 0;
    int cn = 0;
    int sz = 0;
    int type = 0;
    int zero_point = 0;
    float scale = 0.;

    unsigned char *old_buffer = NULL;
    float *new_buffer = NULL;

    for (i = 0; i < in_data->num; i++)
    {
        sz = in_data->out[i].size;//1001
        type = in_data->out[i].param->data_format;
        zero_point = in_data->out[i].param->quant_data.affine.zeroPoint;
        scale = in_data->out[i].param->quant_data.affine.scale;

        old_buffer = in_data->out[i].buf;
        row = in_data->out[i].param->sizes[1];
        col = in_data->out[i].param->sizes[2];
        cn = in_data->out[i].param->sizes[3];

        new_buffer = (float *)out_data->out[i].buf;

        if (1 == cn)
        {
            for (j = 0; j < sz; j++)
            {
                if (NN_BUFFER_FORMAT_UINT8 == type)
                {
                    new_buffer[j] = (float)(((uint8_t*)old_buffer)[j] - zero_point) * scale;
                }
                else if (NN_BUFFER_FORMAT_INT8 == type)
                {
                    new_buffer[j] = (float)(((int8_t*)old_buffer)[j] - zero_point) * scale;
                }
            }
        }
        else
        {
            for (j = 0; j < cn; j++)
            {
                for (k = 0; k < row*col; k++)
                {
                    if (NN_BUFFER_FORMAT_UINT8 == type)
                    {
                        *(new_buffer + k + j*row*col) = (float)(*((uint8_t *)old_buffer + k*cn + j) - zero_point) * scale;
                    }
                    else if (NN_BUFFER_FORMAT_INT8 == type)
                    {
                        *(new_buffer + k + j*row*col) = (float)(*((int8_t *)old_buffer + k*cn + j) - zero_point) * scale;
                    }
                }
            }
        }

        out_data->out[i].size = sz * sizeof(float);
    }
}


void pp_nchw_f32_onescale(nn_output *out_data, nn_output *in_data, int index)
{
    unsigned int i = 0;
    int j = 0;
    int k = 0;
    int row = 0;
    int col = 0;
    int cn = 0;
    int sz = 0;
    int type = 0;
    int zero_point = 0;
    float scale = 0.;

    unsigned char *old_buffer = NULL;
    float *new_buffer = NULL;

	i = index;
    if (i < in_data->num)
    {
        sz = in_data->out[i].size;//1001
        type = in_data->out[i].param->data_format;
        zero_point = in_data->out[i].param->quant_data.affine.zeroPoint;
        scale = in_data->out[i].param->quant_data.affine.scale;

        old_buffer = in_data->out[i].buf;
        row = in_data->out[i].param->sizes[1];
        col = in_data->out[i].param->sizes[2];
        cn = in_data->out[i].param->sizes[3];

        new_buffer = (float *)out_data->out[i].buf;

        if (1 == cn)
        {
            for (j = 0; j < sz; j++)
            {
                if (NN_BUFFER_FORMAT_UINT8 == type)
                {
                    new_buffer[j] = (float)(((uint8_t*)old_buffer)[j] - zero_point) * scale;
                }
                else if (NN_BUFFER_FORMAT_INT8 == type)
                {
                    new_buffer[j] = (float)(((int8_t*)old_buffer)[j] - zero_point) * scale;
                }
            }
        }
        else
        {
            for (j = 0; j < cn; j++)
            {
                for (k = 0; k < row*col; k++)
                {
                    if (NN_BUFFER_FORMAT_UINT8 == type)
                    {
                        *(new_buffer + k + j*row*col) = (float)(*((uint8_t *)old_buffer + k*cn + j) - zero_point) * scale;
                    }
                    else if (NN_BUFFER_FORMAT_INT8 == type)
                    {
                        *(new_buffer + k + j*row*col) = (float)(*((int8_t *)old_buffer + k*cn + j) - zero_point) * scale;
                    }
                }
            }
        }

        //out_data->out[i].size = sz * sizeof(float);
    }
}



void copy_buf(nn_output *out_data, nn_output *in_data)
{
    unsigned int i = 0;

    out_data->num = in_data->num;
    for (i = 0; i < out_data->num; i++)
    {
        out_data->out[i].size = in_data->out[i].size;
        out_data->out[i].out_format = in_data->out[i].out_format;

        out_data->out[i].param->sizes[0] = in_data->out[i].param->sizes[0];
        out_data->out[i].param->sizes[1] = in_data->out[i].param->sizes[1];
        out_data->out[i].param->sizes[2] = in_data->out[i].param->sizes[2];
        out_data->out[i].param->sizes[3] = in_data->out[i].param->sizes[3];
        out_data->out[i].param->quant_data.affine.scale = in_data->out[i].param->quant_data.affine.scale;
        out_data->out[i].param->quant_data.affine.zeroPoint = in_data->out[i].param->quant_data.affine.zeroPoint;
        out_data->out[i].param->data_format = in_data->out[i].param->data_format;
        out_data->out[i].param->num_of_dims = in_data->out[i].param->num_of_dims;

        memcpy(out_data->out[i].buf, in_data->out[i].buf, out_data->out[i].size);
    }
}


