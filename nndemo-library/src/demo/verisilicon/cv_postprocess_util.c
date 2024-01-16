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
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include "nn_util.h"
#define FLT_MAX 3.402823466e+38F
/** Status enum */
typedef enum
{
    UTIL_FAILURE = -1,
    UTIL_SUCCESS = 0,
}nn_status_e;
/*-------------------------------------------
                  Functions
-------------------------------------------*/
float overlap(float x1, float w1, float x2, float w2)
{
    float l1 = x1 - w1/2;
    float l2 = x2 - w2/2;
    float left = l1 > l2 ? l1 : l2;
    float r1 = x1 + w1/2;
    float r2 = x2 + w2/2;
    float right = r1 < r2 ? r1 : r2;
    return right - left;
}

float box_intersection(box a, box b)
{
    float area = 0;
    float w = overlap(a.x, a.w, b.x, b.w);
    float h = overlap(a.y, a.h, b.y, b.h);
    if (w < 0 || h < 0)
        return 0;
    area = w*h;
    return area;
}

float box_union(box a, box b)
{
    float i = box_intersection(a, b);
    float u = a.w*a.h + b.w*b.h - i;
    return u;
}

float box_iou(box a, box b)
{
    return box_intersection(a, b)/box_union(a, b);
}

int nms_comparator(const void *pa, const void *pb)
{
    sortable_bbox a = *(sortable_bbox *)pa;
    sortable_bbox b = *(sortable_bbox *)pb;

    LOGD("a.index=%d, a.classId=%d, b.index=%d, b.classId=%d, a.probs=%p, b.probs=%p\n", a.index, a.classId, b.index,b.classId, a.probs,b.probs);
    LOGD("a.probs = %f\n", a.probs[a.index][b.classId]);
    LOGD("b.probs = %f\n", b.probs[b.index][b.classId]);

    float diff = a.probs[a.index][b.classId] - b.probs[b.index][b.classId];
    if (diff < 0) return 1;
    else if(diff > 0) return -1;
    return 0;
}

void do_nms_sort(box *boxes, float **probs, int total, int classes, float thresh)
{
    int i, j, k;
    sortable_bbox *s = (sortable_bbox *)calloc(total, sizeof(sortable_bbox));

    for (i = 0; i < total; ++i)
    {
        s[i].index = i;
        s[i].classId = 0;
        s[i].probs = probs;
    }

    for (k = 0; k < classes; ++k)
    {
        for (i = 0; i < total; ++i)
        {
            s[i].classId = k;
        }
        qsort(s, total, sizeof(sortable_bbox), nms_comparator);

        for (i = 0; i < total; ++i)
        {
            if (probs[s[i].index][k] == 0)
            {
                continue;
            }

            for (j = i+1; j < total; ++j)
            {
                box b = boxes[s[j].index];
                if (probs[s[j].index][k]>0)
                {
                    if (box_iou(boxes[s[i].index], b) > thresh)
                    {
                        probs[s[j].index][k] = 0;
                    }
                }
            }
        }
    }
    free(s);
}

void flatten(float *x, int size, int layers, int batch, int forward)
{
    float *swap = (float*)calloc(size*layers*batch, sizeof(float));
    int i,c,b;
    for (b = 0; b < batch; ++b)
    {
        for (c = 0; c < layers; ++c)
        {
            for (i = 0; i < size; ++i)
            {
                int i1 = b*layers*size + c*size + i;
                int i2 = b*layers*size + i*layers + c;
                if (forward) swap[i2] = x[i1];
                else swap[i1] = x[i2];
            }
        }
    }
    memcpy(x, swap, size*layers*batch*sizeof(float));
    free(swap);
}

void softmax(float *input, int n, float temp, float *output)
{
    int i;
    float sum = 0;
    float largest = -FLT_MAX;
    for (i = 0; i < n; ++i)
    {
        if (input[i] > largest) largest = input[i];
    }
    for (i = 0; i < n; ++i)
    {
        float e = exp(input[i]/temp - largest/temp);
        sum += e;
        output[i] = e;
    }
    for (i = 0; i < n; ++i)
    {
        output[i] /= sum;
    }
}

float sigmod(float x)
{
    return 1.0/(1+exp(-x));
}

float logistic_activate(float x)
{
    return 1./(1. + exp(-x));
}


unsigned char *transpose(const unsigned char * src,int width,int height)
{
    unsigned char* dst;
    int i,j,m;
    int channel = 3;

    dst = (unsigned char*)malloc(width*height*channel);
    memset(dst,0,width*height*channel);

    /*hwc -> whc*/
    for (i = 0;i < width; i++)
    {
        for (j = 0; j < height; j++)
        {
            for (m = 0;m < channel;m++)
                *(dst + i * height * channel + j * channel + m) = *(src + j * width * channel + i * channel + m);
        }
    }
    return dst;
}

int entry_index(int lw, int lh, int lclasses, int loutputs, int batch, int location, int entry)
{
    int n = location / (lw*lh);
    int loc = location % (lw*lh);
    return batch * loutputs + n * lw*lh*(4 + lclasses + 1) + entry * lw*lh + loc;
}

void activate_array(float *start, int num)
{
    for (int i = 0; i < num; i ++) {
        start[i] = logistic_activate(start[i]);
    }
}

