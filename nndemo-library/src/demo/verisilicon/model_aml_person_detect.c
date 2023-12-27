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
#define WIDTH 640
#define HIGHT 384

float p_bbox_32[8][12][20]; //384/32,640/32
float p_bbox_16[8][24][40];
float p_bbox_8[8][48][80];
//prob score,input
float p_prob_32[480][2];
float p_prob_16[1920][2];
float p_prob_8[7680][2];

static float p_prob32[480][1];
static float p_prob16[1920][1];
static float p_prob8[7680][1];
//output box
static box p_box32[12][20][2];
static box *p_pbox32;
static box p_box16[24][40][2];
static box *p_pbox16;
static box p_box8[48][80][2];
static box *p_pbox8;
extern int g_detect_number;

typedef struct{
    int index;
    int classId;
    float probs;  //**probs to probs
} sortable_bbox_person;

int person_nms_comparator(const void *pa, const void *pb)
{
    sortable_bbox_person a = *(sortable_bbox_person *)pa;
    sortable_bbox_person b = *(sortable_bbox_person *)pb;
    float diff = a.probs - b.probs;
    if (diff < 0) return 1;
    else if (diff > 0) return -1;
    return 0;
}

void person_do_nms_sort(box *boxes, float probs[][1], int total, int classes, float thresh)
{
    int i, j, k;
    sortable_bbox_person *s = (sortable_bbox_person *)calloc(total, sizeof(sortable_bbox_person));
	if (s == NULL)
	{
		printf("terrible calloc fail\n");
		return;
	}
    for (i = 0; i < total; ++i)
    {
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
        qsort(s, total, sizeof(sortable_bbox_person), person_nms_comparator);
        for (i = 0; i < total; ++i)
		{
            if (probs[s[i].index][k] <= 0.02)  //zxw
			{
				probs[s[i].index][k] = 0; //zxw;
				continue;
			}
            for (j = i+1; j < total; ++j)
            {
                box b = boxes[s[j].index];
                if (retina_box_iou(boxes[s[i].index], b) > thresh)
                {
                    probs[s[j].index][k] = 0;
                }
            }
        }
    }
    free(s);
}

void person_set_detections(int num, float thresh, box *boxes, float probs[][1],person_detect_out_t* pperson_detect_result)
{
	int i;
	int detect_num = pperson_detect_result->detNum;

	for (i = 0; i < num; i++)
    {
        float prob = probs[i][0];
        if (detect_num < MAX_DETECT_NUM)
        {
            if (prob > thresh)
            {
				if (detect_num >= g_detect_number)
				{
					break;
				}
                float left = 0;
                float right = 0;
                float top = 0;
                float bot = 0;

                left  = boxes[i].x / 640.0;
                right = (boxes[i].x + boxes[i].w) / 640.0;
                top   = boxes[i].y / 384.0;
                bot   = (boxes[i].y + boxes[i].h) / 384.0;
                if (left < 0) left = 0;
                if (right > 1) right = 1.0;
                if (top < 0) top = 0;
                if (bot > 1) bot = 1.0;

                pperson_detect_result->pBox[detect_num].score = prob;
                pperson_detect_result->pBox[detect_num].x = boxes[i].x / 640.0;
                pperson_detect_result->pBox[detect_num].y = boxes[i].y / 384.0;
                pperson_detect_result->pBox[detect_num].w = boxes[i].w / 640.0;
                pperson_detect_result->pBox[detect_num].h = boxes[i].h / 384.0;

                if (pperson_detect_result->pBox[detect_num].x <= 0 ) pperson_detect_result->pBox[detect_num].x =0.000001;
                if (pperson_detect_result->pBox[detect_num].y <= 0 ) pperson_detect_result->pBox[detect_num].y =0.000001;
                if (pperson_detect_result->pBox[detect_num].w <= 0 ) pperson_detect_result->pBox[detect_num].w =0.000001;
                if (pperson_detect_result->pBox[detect_num].h <= 0 ) pperson_detect_result->pBox[detect_num].h =0.000001;
                if (pperson_detect_result->pBox[detect_num].x >= 1 ) pperson_detect_result->pBox[detect_num].x =0.999999;
                if (pperson_detect_result->pBox[detect_num].y >= 1 ) pperson_detect_result->pBox[detect_num].y =0.999999;
                if (pperson_detect_result->pBox[detect_num].w >= 1 ) pperson_detect_result->pBox[detect_num].w =0.999999;
                if (pperson_detect_result->pBox[detect_num].h >= 1 ) pperson_detect_result->pBox[detect_num].h =0.999999;

                detect_num++;
            }
        }
    }
    pperson_detect_result->detNum = detect_num;
}

int person_do_post_process(person_detect_out_t* pperson_detect_result)
{
	int i = 0,x,y;
    int idx;
    int h32;
	float prior_w,prior_h;
	float prior_cw,prior_ch;
	int pred_ctrx,pred_ctry,predw,predh;
	int valid_8 = 0,valid_16 = 0,valid_32 = 0;

	p_pbox32 = (box *)p_box32;
	p_pbox16 = (box *)p_box16;
	p_pbox8 = (box *)p_box8;

	for (i=0;i<480;i++)
	{
		p_prob32[i][0] = p_prob_32[i][1];
		if (p_prob32[i][0] > 0.8)
			valid_32 = 1;
		else
			p_prob32[i][0]=0;
	}

	for (i=0;i<1920;i++)
	{
		p_prob16[i][0] = p_prob_16[i][1];
		if (p_prob16[i][0] > 0.8)
			valid_16 = 1;
		else
			p_prob16[i][0]=0;
	}

	for (i=0;i<7680;i++)
	{
		p_prob8[i][0] = p_prob_8[i][1];
		if (p_prob8[i][0] > 0.8)
			valid_8 = 1;
		else
			p_prob8[i][0]=0;
	}

	if (valid_32)
	{
		for (y=0;y<12;y++)
		{
			for (x=0;x<20;x++)
			{
				for (idx=0;idx<2;idx++)
				{
					if (idx == 0)
						h32=256;
					else
						h32=512;

					prior_w = (float)h32;
					prior_h = h32*2.44;
					prior_cw = (x+0.5)*32;
					prior_ch = (y+0.5)*32;

					pred_ctrx = p_bbox_32[4*idx][y][x]*0.1*prior_w+prior_cw;
					pred_ctry = p_bbox_32[4*idx+1][y][x]*0.1*prior_h+prior_ch;
					predw = exp(p_bbox_32[4*idx+2][y][x]*0.2)*prior_w;
					predh = exp(p_bbox_32[4*idx+3][y][x]*0.2)*prior_h;
					p_box32[y][x][idx].x = pred_ctrx-0.5*predw;
					p_box32[y][x][idx].y = pred_ctry-0.5*predh;
					p_box32[y][x][idx].w = predw;
					p_box32[y][x][idx].h = predh;
				}
			}
		}
	}

	if (valid_16)
	{
		for (y=0;y<24;y++)
		{
			for (x=0;x<40;x++)
			{
				for (idx=0;idx<2;idx++)
				{
					if (idx == 0)
						h32=64;
					else
						h32=128;

					prior_w = (float)h32;
					prior_h = h32*2.44;
					prior_cw = (x+0.5)*16;
					prior_ch = (y+0.5)*16;

					pred_ctrx = p_bbox_16[4*idx][y][x]*0.1*prior_w+prior_cw;
					pred_ctry = p_bbox_16[4*idx+1][y][x]*0.1*prior_h+prior_ch;
					predw = exp(p_bbox_16[4*idx+2][y][x]*0.2)*prior_w;
					predh = exp(p_bbox_16[4*idx+3][y][x]*0.2)*prior_h;
					p_box16[y][x][idx].x = pred_ctrx-0.5*predw;
					p_box16[y][x][idx].y = pred_ctry-0.5*predh;
					p_box16[y][x][idx].w = predw;
					p_box16[y][x][idx].h = predh;
				}
			}
		}
	}

	if (valid_8)
	{
		for (y=0;y<48;y++)
		{
			for (x=0;x<80;x++)
			{
				for (idx=0;idx<2;idx++)
				{
					if (idx == 0)
						h32=16;
					else
						h32=32;

					prior_w = (float)h32;
					prior_h = h32*2.44;
					prior_cw = (x+0.5)*8;
					prior_ch = (y+0.5)*8;

					pred_ctrx = p_bbox_8[4*idx][y][x]*prior_w*0.1+prior_cw;
					pred_ctry = p_bbox_8[4*idx+1][y][x]*prior_h*0.1+prior_ch;
					predw = exp(p_bbox_8[4*idx+2][y][x]*0.2)*prior_w;
					predh = exp(p_bbox_8[4*idx+3][y][x]*0.2)*prior_h;
					p_box8[y][x][idx].x = (pred_ctrx-0.5*predw);
					p_box8[y][x][idx].y = (pred_ctry-0.5*predh);
					p_box8[y][x][idx].w = predw;
					p_box8[y][x][idx].h = predh;
				}
			}
		}
	}

	if (valid_32 == 1)
	{
		person_do_nms_sort(p_pbox32, p_prob32, 480, 1, 0.4);
		if (valid_16 == 1)
		{
			person_do_nms_sort(p_pbox16, p_prob16, 1920, 1, 0.4);
			do_global_sort(p_pbox32,p_pbox16,p_prob32,p_prob16,480,1920,0.7);
			if (valid_8 == 1)
			{
				person_do_nms_sort(p_pbox8, p_prob8, 7680, 1, 0.2);
				do_global_sort(p_pbox16, p_pbox8, p_prob16, p_prob8, 1920,7680,0.7);
				person_set_detections(480, 0.6, p_pbox32, p_prob32, pperson_detect_result);
				person_set_detections(1920, 0.6, p_pbox16, p_prob16, pperson_detect_result);
				person_set_detections(7680, 0.6, p_pbox8, p_prob8, pperson_detect_result);
			}
			else
			{
				person_set_detections(480, 0.6, p_pbox32, p_prob32, pperson_detect_result);
				person_set_detections(1920, 0.6, p_pbox16, p_prob16, pperson_detect_result);
			}
		}
		else person_set_detections(480, 0.6, p_pbox32, p_prob32, pperson_detect_result);
	}
	if (valid_16 == 1 && valid_32 == 0 )
	{
		person_do_nms_sort(p_pbox16, p_prob16, 1920, 1, 0.4);
		if (valid_8 == 1)
		{
			person_do_nms_sort(p_pbox8, p_prob8, 7680, 1, 0.4);
			do_global_sort(p_pbox16, p_pbox8, p_prob16, p_prob8, 1920,7680,0.7);
			person_set_detections(1920, 0.6, p_pbox16, p_prob16, pperson_detect_result);
			person_set_detections(7680, 0.6, p_pbox8, p_prob8, pperson_detect_result);
		}
		else
			person_set_detections(1920, 0.6, p_pbox16, p_prob16, pperson_detect_result);
	}

	if (valid_8 == 1 && valid_16 == 0 && valid_32 == 0 )
	{
		person_do_nms_sort(p_pbox8, p_prob8, 7680, 1, 0.2);
		person_set_detections(7680, 0.6, p_pbox8, p_prob8, pperson_detect_result);
	}

	return 0;
}