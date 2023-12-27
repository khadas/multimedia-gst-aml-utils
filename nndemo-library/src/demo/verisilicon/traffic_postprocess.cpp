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

#include "cv_postprocess.h"
#include "nn_sdk.h"
#include "nn_util.h"

#ifndef ANDROID_SDK
#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <memory.h>
using namespace std;
#endif
extern int g_detect_number;

//car plate detect model
static float plate_det_bbox_32[8][9][16]; //512/32,640/32
static float plate_det_bbox_16[8][18][32];
static float plate_det_bbox_8[8][36][64];
//prob score,input
static float plate_det_prob_32[288][2];
static float plate_det_prob_16[1152][2];
static float plate_det_prob_8[4608][2];

static float plate_det_prob32[288][1];
static float plate_det_prob16[1152][1];
static float plate_det_prob8[4608][1];

#if 1
static float  plate_det_land_32[16][9][16];
static float  plate_det_land_16[16][18][32];
static float  plate_det_land_8[16][36][64];
#else
static float  plate_det_land_32[16][9][16];
static float  plate_det_land_16[32][18][16];
static float  plate_det_land_8[64][36][16];

#endif

//output box
static plate_box plate_det_box32[9][16][2];
static plate_box *plate_det_pbox32;
static plate_box plate_det_box16[18][32][2];
static plate_box *plate_det_pbox16;
static plate_box plate_det_box8[36][64][2];
static plate_box *plate_det_pbox8;

//landmark
static landmark plate_det_land32[9][16][2][4];
static landmark *plate_det_pland32;
static landmark plate_det_land16[18][32][2][4];
static landmark *plate_det_pland16;
static landmark plate_det_land8[36][64][2][4];
static landmark *plate_det_pland8;

extern const int PLATE_WIDTH = 128;
extern const int PLATE_HEIGHT = 32;
extern const int PLATE_CHANNEL = 3;
extern const int PLATE_OUTPUT_STEPS = 28;
extern const int PLATE_OUTPUT_CLASSES = 77;

void set_carplate_detections(int num, float thresh, plate_box *boxes, float probs[][1], landmark *pland, plate_det_out_t* pplate_det_result)
{
    int i;
    //float left = 0, right = 0, top = 0, bot=0;
    int detect_num = pplate_det_result->detNum;
    //printf("detect_num = %d\n", detect_num);
    float prob ;
    for (i = 0; i < num; i++)
    {
        prob = probs[i][0];
        if (detect_num < MAX_DETECT_NUM)
        {
            if (prob > thresh)
            {
                if (detect_num >= g_detect_number)
                {
                    break;
                }
                //left  = boxes[i].x ; // 512.0;
                //right = (boxes[i].x + boxes[i].w); // 512.0;
                //top   = boxes[i].y; // 288.0;
                //bot   = (boxes[i].y + boxes[i].h); // 288.0;
                pplate_det_result->pBox[detect_num].score = prob;
                pplate_det_result->pBox[detect_num].x = boxes[i].x / 512.0;
                pplate_det_result->pBox[detect_num].y = boxes[i].y / 288.0;
                pplate_det_result->pBox[detect_num].w = boxes[i].w / 512.0;
                pplate_det_result->pBox[detect_num].h = boxes[i].h / 288.0;
                //printf("detect x = %f,detect y = %f,detect w = %f,detect h = %f,\n",pplate_det_result->pBox[detect_num].x, pplate_det_result->pBox[detect_num].y,pplate_det_result->pBox[detect_num].w,pplate_det_result->pBox[detect_num].h);
                pplate_det_result->score = prob;

                for (int j=0 ;j <4; j++)
                {
                    pplate_det_result->pos[detect_num][j].x = pland[i * 4 + j].x / 512.0;
                    pplate_det_result->pos[detect_num][j].y = pland[i * 4 + j].y / 288.0;
                }
                detect_num++;
            }
        }
    }
    pplate_det_result->detNum = detect_num;
}

static float overlap_plate(float x1, float w1, float x2, float w2)
{
    float l1 = x1 - w1 / 2;
    float l2 = x2 - w2 / 2;
    float left = l1 > l2 ? l1 : l2;
    float r1 = x1 + w1 / 2;
    float r2 = x2 + w2 / 2;
    float right = r1 < r2 ? r1 : r2;
    return right - left;
}

static float box_intersection_plate(plate_box a, plate_box b)
{
    float w = overlap_plate(a.x, a.w, b.x, b.w);
    float h = overlap_plate(a.y, a.h, b.y, b.h);
    if (w < 0 || h < 0) return 0;
    float area = w * h;
    return area;
}

static float box_union_plate(plate_box a, plate_box b)
{
    float i = box_intersection_plate(a, b);
    float u = a.w*a.h + b.w*b.h - i;
    return u;
}

static float box_iou_plate(plate_box a, plate_box b)
{
    return box_intersection_plate(a, b) / box_union_plate(a, b);
}

static int nms_comparator_plate(const void *pa, const void *pb)
{
    sortable_bbox_plate a = *(sortable_bbox_plate *)pa;
    sortable_bbox_plate b = *(sortable_bbox_plate *)pb;
    float diff =a.probs - b.probs;
    if (diff < 0) return 1;
    else if (diff > 0) return -1;
    return 0;
}

void do_global_sort_plate(plate_box *boxe1,plate_box *boxe2, float prob1[][1],float prob2[][1], int len1,int len2,float thresh)
{
    int i,j;
    for (i = 0; i < len1; ++i)
    {
        if (prob1[i][0] > thresh)
        {
            for (j = 0;j < len2;j++)
            {
                if (prob2[j][0] > thresh)
                {
                    if (box_iou_plate(boxe1[i], boxe2[j]) > 0.1)
                    {
                        if (prob2[j][0] > prob1[i][0])
                        {
                            prob1[i][0] = 0;
                        }
                        else
                        {
                            prob2[j][0] = 0;
                        }
                    }
                }
            }
        }
    }
}

void do_nms_sort_plate(plate_box *boxes, float probs[][1], int total, int classes, float thresh)
{
    int i, j, k;

    sortable_bbox_plate *s = (sortable_bbox_plate *)calloc(total, sizeof(sortable_bbox_plate));
    if (s == NULL)
    {
        printf("terrible calloc fail\n");
        return;
    }
    for (i = 0; i < total; ++i) {
        s[i].index = i;
        s[i].classId = 0;
        s[i].probs = probs[i][0];
    }

    for (k = 0; k < classes; ++k)
    {
        for (i = 0; i < total; ++i)
        {
            s[i].classId = k;
        }
        //printf("k:%d,total:%d\n",k,total);
        qsort(s, total, sizeof(sortable_bbox_plate), nms_comparator_plate);
        //printf("qsort after,index:%d,probs:%f\n",s[0].index,s[0].probs);
        for (i = 0; i < total; ++i)
        {
            if (probs[s[i].index][k] <= 0.02)  //zxw
            {
                probs[s[i].index][k] = 0; //zxw;
                continue;
            }
            for (j = i+1; j < total; ++j) {
                plate_box b = boxes[s[j].index];
                if (box_iou_plate(boxes[s[i].index], b) > thresh) {
                    probs[s[j].index][k] = 0;
                }
            }
        }
    }
    free(s);
}

int postprocess_plate_det(plate_det_out_t* pplate_det_result)
{
    int i = 0,x,y;
    int idx;
    int h32=0,h16=0,h8=0;
    float prior_w,prior_h;
    float prior_cw,prior_ch;
    int pred_ctrx,pred_ctry,predw,predh;
    int valid_8 = 0,valid_16 = 0,valid_32 = 0;

    plate_det_pbox32 = (plate_box *)plate_det_box32;
    plate_det_pbox16 = (plate_box *)plate_det_box16;
    plate_det_pbox8 = (plate_box *)plate_det_box8;

    plate_det_pland32 = (landmark *)plate_det_land32;
    plate_det_pland16 = (landmark *)plate_det_land16;
    plate_det_pland8 = (landmark *)plate_det_land8;

    for (i=0;i<288;i++)
    {
        plate_det_prob32[i][0] = plate_det_prob_32[i][1];
        if (plate_det_prob32[i][0] > 0.8)
            valid_32 = 1;
        else
            plate_det_prob32[i][0]=0;
    }

    for (i=0;i<1152;i++)
    {
        plate_det_prob16[i][0] = plate_det_prob_16[i][1];
        if (plate_det_prob16[i][0] > 0.8)
            valid_16 = 1;
        else
            plate_det_prob16[i][0]=0;
    }

    for (i=0;i<4608;i++)
    {
        plate_det_prob8[i][0] = plate_det_prob_8[i][1];
        if (plate_det_prob8[i][0] > 0.8)
            valid_8 = 1;
        else
            plate_det_prob8[i][0]=0;
    }

    if (valid_32)
    {
        for (y=0;y<9;y++)
        {
            for (x=0;x<16;x++)
            {
                for (idx=0;idx<2;idx++)
                {
                    if (idx == 0)
                        h32=256;
                    else
                        h32=512;

                    prior_w = (float)h32;
                    prior_h = h32/2.7;
                    prior_cw = (x+0.5)*32;
                    prior_ch = (y+0.5)*32;


                    pred_ctrx = plate_det_bbox_32[4*idx][y][x]*0.1*prior_w+prior_cw;
                    pred_ctry = plate_det_bbox_32[4*idx+1][y][x]*0.1*prior_h+prior_ch;
                    predw = exp(plate_det_bbox_32[4*idx+2][y][x]*0.2)*prior_w;
                    predh = exp(plate_det_bbox_32[4*idx+3][y][x]*0.2)*prior_h;
                    plate_det_box32[y][x][idx].x = pred_ctrx-0.5*predw;
                    plate_det_box32[y][x][idx].y = pred_ctry-0.5*predh;
                    plate_det_box32[y][x][idx].w = predw;
                    plate_det_box32[y][x][idx].h = predh;
                    for (i=0;i<4;i++)
                    {
                        plate_det_land32[y][x][idx][i].x=(plate_det_land_32[8*idx+2*i][y][x])*0.1*prior_w+prior_cw;
                        plate_det_land32[y][x][idx][i].y=(plate_det_land_32[8*idx+2*i+1][y][x])*0.1*prior_h+prior_ch;
                    }
                }
            }
        }
    }

    if (valid_16)
    {
        for (y=0;y<18;y++)
        {
            for (x=0;x<32;x++)
            {
                for (idx=0;idx<2;idx++)
                {
                    if (idx == 0)
                        h16=64;
                    else
                        h16=128;

                    prior_w = (float)h16;
                    prior_h = h16/2.7;
                    prior_cw = (x+0.5)*16;
                    prior_ch = (y+0.5)*16;

                    pred_ctrx = plate_det_bbox_16[4*idx][y][x]*0.1*prior_w+prior_cw;
                    pred_ctry = plate_det_bbox_16[4*idx+1][y][x]*0.1*prior_h+prior_ch;
                    predw = exp(plate_det_bbox_16[4*idx+2][y][x]*0.2)*prior_w;
                    predh = exp(plate_det_bbox_16[4*idx+3][y][x]*0.2)*prior_h;
                    plate_det_box16[y][x][idx].x = pred_ctrx-0.5*predw;
                    plate_det_box16[y][x][idx].y = pred_ctry-0.5*predh;
                    plate_det_box16[y][x][idx].w = predw;
                    plate_det_box16[y][x][idx].h = predh;
                    for (i=0;i<4;i++)
                    {
                        plate_det_land16[y][x][idx][i].x=(plate_det_land_16[8*idx+2*i][y][x])*0.1*prior_w+prior_cw;
                        plate_det_land16[y][x][idx][i].y=(plate_det_land_16[8*idx+2*i+1][y][x])*0.1*prior_h+prior_ch;
                    }
                }
            }
        }
    }

    if (valid_8)
    {
        for (y=0;y<36;y++)
        {
            for (x=0;x<64;x++)
            {
                for (idx=0;idx<2;idx++)
                {
                    if (idx == 0)
                        h8=24;
                    else
                        h8=32;

                    prior_w = (float)h8;
                    prior_h = h8/2.7;
                    prior_cw = (x+0.5)*8;
                    prior_ch = (y+0.5)*8;

                    pred_ctrx = plate_det_bbox_8[4*idx][y][x]*prior_w*0.1+prior_cw;
                    pred_ctry = plate_det_bbox_8[4*idx+1][y][x]*prior_h*0.1+prior_ch;
                    predw = exp(plate_det_bbox_8[4*idx+2][y][x]*0.2)*prior_w;
                    predh = exp(plate_det_bbox_8[4*idx+3][y][x]*0.2)*prior_h;
                    plate_det_box8[y][x][idx].x = (pred_ctrx-0.5*predw);
                    plate_det_box8[y][x][idx].y = (pred_ctry-0.5*predh);
                    plate_det_box8[y][x][idx].w = predw;
                    plate_det_box8[y][x][idx].h = predh;
                    for (i=0;i<4;i++)
                    {
                        plate_det_land8[y][x][idx][i].x=(plate_det_land_8[8*idx+2*i][y][x])*0.1*prior_w+prior_cw;
                        plate_det_land8[y][x][idx][i].y=(plate_det_land_8[8*idx+2*i+1][y][x])*0.1*prior_h+prior_ch;
                    }
                }
            }
        }
    }

    //printf("valid_32 = %d, valid_16 = %d, valid_8 = %d\n", valid_32,valid_16,valid_8);
    if (valid_32 == 1) {
        do_nms_sort_plate(plate_det_pbox32, plate_det_prob32, 288, 1, 0.1);
        if (valid_16 == 1) {
            do_nms_sort_plate(plate_det_pbox16, plate_det_prob16, 1152, 1, 0.1);
            do_global_sort_plate(plate_det_pbox32,plate_det_pbox16,plate_det_prob32,plate_det_prob16,288,1152,0.8);
            if (valid_8 == 1) {
                do_nms_sort_plate(plate_det_pbox8, plate_det_prob8, 4608, 1, 0.1);
                do_global_sort_plate(plate_det_pbox16, plate_det_pbox8, plate_det_prob16, plate_det_prob8, 1152,4608,0.8);
                set_carplate_detections(288, 0.6, plate_det_pbox32, plate_det_prob32, plate_det_pland32, pplate_det_result);
                set_carplate_detections(1152, 0.6, plate_det_pbox16, plate_det_prob16, plate_det_pland16, pplate_det_result);
                set_carplate_detections(4508, 0.6, plate_det_pbox8, plate_det_prob8, plate_det_pland8, pplate_det_result);
            }
            else {
                set_carplate_detections(288, 0.6, plate_det_pbox32, plate_det_prob32, plate_det_pland32, pplate_det_result);
                set_carplate_detections(1152, 0.6, plate_det_pbox16, plate_det_prob16, plate_det_pland16, pplate_det_result);
            }
        }
        else set_carplate_detections(288, 0.6, plate_det_pbox32, plate_det_prob32, plate_det_pland32, pplate_det_result);
    }

    if (valid_16 == 1 && valid_32 == 0 )
    {
        do_nms_sort_plate(plate_det_pbox16, plate_det_prob16, 1152, 1, 0.1);
        if (valid_8 == 1)
        {
            do_nms_sort_plate(plate_det_pbox8, plate_det_prob8, 4608, 1, 0.1);
            do_global_sort_plate(plate_det_pbox16, plate_det_pbox8, plate_det_prob16, plate_det_prob8, 1152,4608,0.8);
            set_carplate_detections(1152, 0.6, plate_det_pbox16, plate_det_prob16, plate_det_pland16, pplate_det_result);
            set_carplate_detections(4608, 0.6, plate_det_pbox8, plate_det_prob8, plate_det_pland8, pplate_det_result);
        }
        else {
            set_carplate_detections(1152, 0.6, plate_det_pbox16, plate_det_prob16, plate_det_pland16, pplate_det_result);

        }

    }
    if (valid_8 == 1 && valid_16 == 0 && valid_32 == 0 ) {
        do_nms_sort_plate(plate_det_pbox8, plate_det_prob8, 4608, 1, 0.2);
        set_carplate_detections(4608, 0.6, plate_det_pbox8, plate_det_prob8, plate_det_pland8, pplate_det_result);
    }
    return 0;
}

void* postprocess_plate_detect(nn_output *pout)
{
    float *buffer;
    unsigned int sz ;
    unsigned int i;
    static plate_det_out_t plate_det_result;
    memset(&plate_det_result,0,sizeof(point_t));
    for (i=0;i<pout->num;i++)
    {
        buffer = (float *)pout->out[i].buf;
        sz= pout->out[i].size;
        switch (i)
        {
            case 0:
                memcpy(plate_det_bbox_8,buffer,sz);
                break;
            case 1:
                memcpy(plate_det_bbox_16,buffer,sz);
                break;
            case 2:
                memcpy(plate_det_bbox_32,buffer,sz);
                break;
            case 3:
                memcpy(plate_det_prob_8,buffer,sz);
                break;
            case 4:
                memcpy(plate_det_prob_16,buffer,sz);
                break;
            case 5:
                memcpy(plate_det_prob_32,buffer,sz);
                break;
            case 6:
                memcpy(plate_det_land_8,buffer,sz);
                break;
            case 7:
                memcpy(plate_det_land_16,buffer,sz);
                break;
            case 8:
                memcpy(plate_det_land_32,buffer,sz);
                break;
            default:
                break;
        }
    }
    postprocess_plate_det(&plate_det_result);
    return (void*)(&plate_det_result);
}


void* postprocess_plate_recognize(nn_output *pout)
{
    static plate_recog_out_t plate_recog_result;
    memset(&plate_recog_result, 0, sizeof(plate_recog_out_t));

    if (pout->out[0].param->data_format == NN_BUFFER_FORMAT_FP32)
    {
        plate_recog_result.buf = (float*)pout->out[0].buf;
    }
    return (void*)(&plate_recog_result);
}
extern "C"
void* postprocess_carreg(nn_output *pout)
{
    float *buffer;
    int i,j,index;
    unsigned int sz;
    float result_buffer[18][84];
    float buffer_rmfd2[16][84];
    int argmax[16] = {0};
    float max;
    const char *license[] = {"¾©", "»¦", "½ò", "Óå", "¼½", "½ú", "ÃÉ", "ÁÉ", "¼ª", "ºÚ", "ËÕ", "Õã", "Íî", "Ãö", "¸Ó", "Â³", "Ô¥", "¶õ", "Ïæ", "ÔÁ", "¹ð",
             "Çí", "´¨", "¹ó", "ÔÆ", "²Ø", "ÉÂ", "¸Ê", "Çà", "Äþ", "ÐÂ", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A",
             "B", "C", "D", "E", "F", "G", "H", "J", "K", "L", "M", "N", "P", "Q", "R", "S", "T", "U", "V", "W", "X",
             "Y", "Z","¸Û","Ñ§","Ê¹","¾¯","°Ä","¹Ò","¾ü","±±","ÄÏ","¹ã","Éò","À¼","³É","¼Ã","º£","Ãñ","º½","¿Õ",""};
    car_license_out_t *carreg_result = NULL;

    carreg_result = (car_license_out_t*)malloc(sizeof(car_license_out_t));
    memset(carreg_result,0,sizeof(car_license_out_t));
    buffer = (float*)pout->out[0].buf;
    sz = pout->out[0].size;
    memcpy(result_buffer,buffer,sz);

    for (i = 0;i < 16; i++)
    {
        for (j = 0;j < 84;j++)
        buffer_rmfd2[i][j] = result_buffer[i+2][j];
    }

    for (i = 0;i < 16; i++)
    {
        max = buffer_rmfd2[i][0];
        for (j = 1;j < 84;j++)
        {
            if (max < buffer_rmfd2[i][j])
            {
                max = buffer_rmfd2[i][j];
                argmax[i] = j;
            }
        }
    }

    index = 0;
    for (i = 0;i < 16;i++)
    {
        if ((argmax[i] < 84) && (i == 0 || (argmax[i] != argmax[i-1])))
        {
            strcat((char *)carreg_result->val,license[argmax[i]]);
            carreg_result->confidence += buffer_rmfd2[i][argmax[i]];
            index += 1;
        }
    }

    carreg_result->confidence = carreg_result->confidence / index;

    return (void*)carreg_result;
}


#ifdef USE_OPENCV
/*=========================================================================================================
*****************************************text detect postprocess begin*************************************
==========================================================================================================*/
void decode(float scoreThresh,std::vector<RotatedRect>& detections, std::vector<float>& confidences,float* pscore,float*pgeo)
{
    detections.clear();
    confidences.clear();

    const int height = 80;
    const int width = 80;
    for (int y = 0; y < height; ++y)
    {
        const float* scoresData = (pscore+80*y);
        for (int x = 0; x < width; ++x)
        {
            float score = scoresData[x];
            int index = y*400+5*x;
            if (score < scoreThresh)
                continue;
            // Decode a prediction.
            // Multiple by 4 because feature maps are 4 time less than input image.
            float offsetX = x * 4.0f, offsetY = y * 4.0f;
            float angle = pgeo[index+4];
            float cosA = std::cos(angle);
            float sinA = std::sin(angle);

            float h = pgeo[index] + pgeo[index+2];
            float w = pgeo[index+1] + pgeo[index+3];
            Point2f offset(offsetX + cosA * pgeo[index+1] + sinA * pgeo[index+2],
                           offsetY - sinA * pgeo[index+1] + cosA * pgeo[index+2]);
            Point2f p1 = Point2f(-sinA * h, -cosA * h) + offset;
            Point2f p3 = Point2f(-cosA * w, sinA * w) + offset;
            RotatedRect r(0.5f * (p1 + p3), Size2f(w, h), -angle * 180.0f / (float)CV_PI);
            detections.push_back(r);
            confidences.push_back(score);
        }
    }
}

extern "C"
void* post_textdet(float* score,float* geo)
{
    float confThreshold = 0.1;
    float nmsThreshold = 0.4;
    static text_det_out_t textout = {0};
    float min_x = 0.0,min_y = 0.0,max_x = 0.0,max_y = 0.0;
    if ((geo == NULL) || (score == NULL))
    {
        printf("ERROR: input ptr is null\n");
        return NULL;
    }
    std::vector<RotatedRect> boxes;
    std::vector<float> confidences;
    decode(confThreshold, boxes, confidences,score,geo);

    // Apply non-maximum suppression procedure.
    std::vector<int> indices;
    NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);

    // Render detections.
    Point2f ratio(1, 1);
    textout.textOut.detNum = indices.size();
    if (textout.textOut.pBox == NULL)
    {
        textout.textOut.pBox = (detBox*)malloc(sizeof(detBox)*(textout.textOut.detNum));
    }
    if (textout.textOut.pBox == NULL)
    {
        free(score);
        free(geo);
        return NULL;
    }
    for (size_t i = 0; i < indices.size(); ++i)
    {
        RotatedRect& box = boxes[indices[i]];

        Point2f vertices[4];
        box.points(vertices);
        min_x = vertices[0].x;
        min_y = vertices[0].y;

        for (int j = 0; j < 4; ++j)
        {
            vertices[j].x *= ratio.x;
            vertices[j].y *= ratio.y;
            if (min_x > vertices[j].x)
            {
                min_x = vertices[j].x;
            }
            if (min_y > vertices[j].y)
            {
                min_y = vertices[j].y;
            }
            if (max_x < vertices[j].x)
            {
                max_x = vertices[j].x;
            }
            if (max_y < vertices[j].y)
            {
                max_y = vertices[j].y;
            }
            //printf("point %d,x-%f,y-%f\n",j,vertices[j].x,vertices[j].y);
        }
        textout.textOut.pBox[i].x = min_x;
        textout.textOut.pBox[i].y = min_y;
        textout.textOut.pBox[i].w = max_x-min_x;
        textout.textOut.pBox[i].h = max_y-min_y;
    }
    free(score);
    free(geo);
    return &textout;
}


void* postprocess_textdet(nn_output *pout)
{
    float *pscore = (float *)malloc(80*80*sizeof(float));
    float *pgeo= (float *)malloc(80*80*5*sizeof(float));
    memcpy(pscore,pout->out[0].buf,80*80*sizeof(float));
    memcpy(pgeo,pout->out[1].buf,80*80*5*sizeof(float));
    return post_textdet(pscore,pgeo);
}
#endif
