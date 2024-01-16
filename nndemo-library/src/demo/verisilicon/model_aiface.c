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
#include <unistd.h>
#include "nn_sdk.h"
#include "nn_util.h"
#include "cv_postprocess.h"

//==========================================t3=======================================
#define MAX_FACE_DETECT_NUM         100
#define CONFIDENCE_THRESHOLD        0.8
#define NMS_THRESHOLD               0.25
#define GLOBAL_NMS_THRESHOLD        0.7
#define TOP_K                       500
#define KEEP_TOP_K                  100
#define OUTPUT_SIZE                 14280

static float rprob8[10800] = {0};
static float rprob16[2760] = {0};
static float rprob32[720] = {0};
//output box
static detBox rbox8[45][80][3];
static detBox rbox16[23][40][3];
static detBox rbox32[12][20][3];

static float overlap_aiface(float x1, float w1, float x2, float w2)
{
    float l1 = x1;
    float l2 = x2;
    float left = l1 > l2 ? l1 : l2;
    float r1 = x1 + w1;
    float r2 = x2 + w2;
    float right = r1 < r2 ? r1 : r2;
    return right - left;
}

static float box_intersection_aiface(detBox a, detBox b)
{
    float area = 0;
    float w = overlap_aiface(a.x, a.w, b.x, b.w);
    float h = overlap_aiface(a.y, a.h, b.y, b.h);
    if (w < 0 || h < 0) {
        return 0;
    }
    area = w * h;
    return area;
}

static float box_union_aiface(detBox a, detBox b)
{
    float i = box_intersection_aiface(a, b);
    float u = a.w * a.h + b.w * b.h - i;
    return u;
}

static float box_iou_aiface(detBox a, detBox b)
{
    return box_intersection_aiface(a, b) / box_union_aiface(a, b);
}

static int nms_comparator_aiface(const void *pa, const void *pb)
{
    face_sortable_bbox a = *(face_sortable_bbox *)pa;
    face_sortable_bbox b = *(face_sortable_bbox *)pb;
    float diff = a.probs - b.probs;
    if (diff < 0)
        return 1;
    else if (diff > 0)
        return -1;

    return 0;
}

static void do_intersection_aiface(detBox *box_a, detBox *box_b)
{
    float ul_x = fmax(box_a->x, box_b->x);
    float ul_y = fmax(box_a->y, box_b->y);
    float lr_x = fmin(box_a->x + box_a->w, box_b->x + box_b->w);
    float lr_y = fmin(box_a->y + box_a->h, box_b->y + box_b->h);
    box_a->x = ul_x;
    box_a->y = ul_y;
    box_a->w = lr_x - ul_x;
    box_a->h = lr_y - ul_y;
}

static void do_nms_sort_aiface(detBox *boxes, float probs[], int total)
{
    int i = 0, j = 0;
    face_sortable_bbox *s = (face_sortable_bbox *)calloc(total, sizeof(face_sortable_bbox));
    for (i = 0; i < total; ++i) {
        s[i].index = i;
        s[i].classId = 0;
        s[i].probs = probs[i];
    }

    qsort(s, total, sizeof(face_sortable_bbox), nms_comparator_aiface);

    for (i = 0; i < total; ++i) {
        if (probs[s[i].index] >= CONFIDENCE_THRESHOLD) {
            for (j = i+1; j < total; j++) {
                if (probs[s[j].index] >= CONFIDENCE_THRESHOLD) {
                    detBox b = boxes[s[j].index];
                    if (box_iou_aiface(boxes[s[i].index], b) > NMS_THRESHOLD) {
                        if (probs[s[i].index] == probs[s[j].index])
                            do_intersection_aiface(&boxes[s[i].index], &boxes[s[j].index]);
                        probs[s[j].index] = 0;
                    }
                }
            }
        }
    }
    free(s);
}

static void face_set_result_aiface(int num, detBox *boxes, float probs[], face_landmark5_out_t* face_out)
{

    int i;
    int detect_num = face_out->detNum;
    for (i = 0; i < num; i++) {
        float prob = probs[i];
        if (detect_num < MAX_FACE_DETECT_NUM) {
            if (prob > CONFIDENCE_THRESHOLD) {
                if (detect_num >= MAX_FACE_DETECT_NUM)
                    break;
                face_out->facebox[detect_num].score = prob;
                face_out->facebox[detect_num].x = boxes[i].x;
                face_out->facebox[detect_num].y = boxes[i].y;
                face_out->facebox[detect_num].w = boxes[i].w;
                face_out->facebox[detect_num].h = boxes[i].h;
                detect_num++;
            }
        }
    }
    face_out->detNum = detect_num;
}

static void do_global_sort_aiface(detBox *boxe1, detBox *boxe2, float prob1[], float prob2[], int len1, int len2)
{
    int i,j;
    for (i = 0; i < len1; ++i) {
        if (prob1[i] > GLOBAL_NMS_THRESHOLD) {
            for (j = 0; j < len2; j++) {
                if (prob2[j] > GLOBAL_NMS_THRESHOLD) {
                    if (box_iou_aiface(boxe1[i], boxe2[j]) > 0.1) {
                        if (prob2[j] > prob1[i])
                            prob1[i] = 0;
                        else
                            prob2[j] = 0;
                    }
                }
            }
        }
    }
}

static void process_face_detect_aiface(float *bbox_buf, float *prob_buf, face_landmark5_out_t* face_out)
{
    int i = 0, k = 0, m = 0, x = 0, y = 0;
    int h32 = 0, w32 = 0, h16 = 0, w16 = 0, h8 = 0, w8 = 0;
    float pred_ctrx = 0, pred_ctry = 0, predw = 0, predh = 0;
    int valid_8 = 0, valid_16 = 0, valid_32 = 0;

    detBox *rpbox8  = (detBox *)rbox8;
    detBox *rpbox16 = (detBox *)rbox16;
    detBox *rpbox32 = (detBox *)rbox32;

    for (i = 0; i < OUTPUT_SIZE; i++) {
        if (i < 10800) {
            rprob8[m] = prob_buf[2 * i + 1];
            if (rprob8[m] < CONFIDENCE_THRESHOLD)
                rprob8[m] = 0;
            else
                valid_8 = 1;
            m++;
        }
        else if (i < 13560) {
            rprob16[x] = prob_buf[2 * i + 1];
            if (rprob16[x] < CONFIDENCE_THRESHOLD)
                rprob16[x] = 0;
            else
                valid_16 = 1;
            x++;
        }
        else {
            rprob32[y] = prob_buf[2 * i + 1];
            if (rprob32[y] < CONFIDENCE_THRESHOLD)
                rprob32[y] = 0;
            else
                valid_32 = 1;
            y++;
        }
    }

    if (valid_8 == 1) {
        for (y = 0, k = 0; y < 45; y++) {
            for (x = 0; x < 80; x++) {
                for (i = 0; i < 3; i++) {
                    if (i == 0)
                        h8 = w8 = 6;
                    else if (i == 1)
                        h8 = w8 = 10;
                    else
                        h8 = w8 = 16;

                    float s_kx = w8;
                    float s_ky = h8 ;
                    float cx = (x + 0.5) * 8;
                    float cy = (y + 0.5) * 8;

                    pred_ctrx = cx + bbox_buf[k] * 0.1 * s_kx;
                    pred_ctry = cy + bbox_buf[k+1]* 0.1 * s_ky;
                    predw = exp((bbox_buf[k+2])*0.2) * s_kx;
                    predh = exp((bbox_buf[k+3])*0.2) * s_ky;

                    rbox8[y][x][i].x = (pred_ctrx-0.5*(predw));
                    rbox8[y][x][i].y = (pred_ctry-0.5*(predh));
                    rbox8[y][x][i].w = predw;
                    rbox8[y][x][i].h = predh;
                    k += 4;
                }
            }
        }
    }

    if (valid_16 == 1) {
        for (y=0, k = 43200; y < 23; y++) {
            for (x = 0; x < 40; x++) {
               for (i = 0; i < 3; i++) {
                    if (i == 0)
                        h16 = w16 = 24;
                    else if (i == 1)
                        h16 = w16 = 40;
                    else
                        h16 = w16 = 64;

                    float s_kx = w16;
                    float s_ky = h16;
                    float cx = (x + 0.5) * 16;
                    float cy = (y + 0.5) * 16;

                    pred_ctrx = cx + bbox_buf[k] * 0.1 * s_kx;
                    pred_ctry = cy + bbox_buf[k+1]* 0.1 * s_ky;
                    predw = exp((bbox_buf[k+2])*0.2) * s_kx;
                    predh = exp((bbox_buf[k+3])*0.2) * s_ky;

                    rbox16[y][x][i].x = (pred_ctrx-0.5*(predw));
                    rbox16[y][x][i].y = (pred_ctry-0.5*(predh));
                    rbox16[y][x][i].w = predw;
                    rbox16[y][x][i].h = predh;
                    k += 4;
                }
            }
        }
    }

    if (valid_32 == 1) {
        for (y = 0, k = 54240; y < 12; y++) {
            for (x = 0; x < 20; x++) {
                for (i = 0; i < 3; i++) {
                    if (i == 0)
                        h32=w32=96;
                    else if (i == 1)
                        h32=w32=160;
                    else
                        h32=w32=256;

                    float s_kx = w32;
                    float s_ky = h32;
                    float cx = (x + 0.5) * 32;
                    float cy = (y + 0.5) * 32;

                    pred_ctrx = cx + bbox_buf[k] * 0.1 * s_kx;
                    pred_ctry = cy + bbox_buf[k+1]* 0.1 * s_ky;
                    predw = exp((bbox_buf[k+2])*0.2) * s_kx;
                    predh = exp((bbox_buf[k+3])*0.2) * s_ky;

                    rbox32[y][x][i].x = (pred_ctrx-0.5*(predw));
                    rbox32[y][x][i].y = (pred_ctry-0.5*(predh));
                    rbox32[y][x][i].w = predw;
                    rbox32[y][x][i].h = predh;
                    k += 4;
                }
            }
        }
    }

    if (valid_32 == 1) {
        do_nms_sort_aiface(rpbox32, rprob32, 720);
        if (valid_16 == 1) {
            do_nms_sort_aiface(rpbox16, rprob16, 2760);
            do_global_sort_aiface(rpbox32,rpbox16,rprob32,rprob16,720,2760);
            if (valid_8 == 1) {
                do_nms_sort_aiface(rpbox8, rprob8, 10800);
                do_global_sort_aiface(rpbox16, rpbox8, rprob16, rprob8, 2760,10800);
                face_set_result_aiface(720, rpbox32, rprob32, face_out);
                face_set_result_aiface(2760, rpbox16, rprob16, face_out);
                face_set_result_aiface(10800, rpbox8, rprob8, face_out);
            }
            else {
                face_set_result_aiface(720, rpbox32, rprob32, face_out);
                face_set_result_aiface(2760, rpbox16, rprob16, face_out);
            }
        }
        else if (valid_8 == 1 && valid_16 == 0) {
            do_nms_sort_aiface(rpbox8, rprob8, 10800);
            face_set_result_aiface(720, rpbox32, rprob32, face_out);
            face_set_result_aiface(10800, rpbox8, rprob8, face_out);
        }
        else
            face_set_result_aiface(720, rpbox32, rprob32, face_out);
    }

    if (valid_16 == 1 && valid_32 == 0) {
        do_nms_sort_aiface(rpbox16, rprob16, 2760);
        if (valid_8 == 1) {
            do_nms_sort_aiface(rpbox8, rprob8, 10800);
            do_global_sort_aiface(rpbox16, rpbox8, rprob16, rprob8, 2760,10800);
            face_set_result_aiface(2760, rpbox16, rprob16, face_out);
            face_set_result_aiface(10800, rpbox8, rprob8, face_out);
        }
        else
            face_set_result_aiface(2760, rpbox16, rprob16, face_out);
    }

    if (valid_8 == 1 && valid_16 == 0 && valid_32 == 0) {
        do_nms_sort_aiface(rpbox8, rprob8, 10800);
        face_set_result_aiface(10800, rpbox8, rprob8, face_out);
    }
}

static void post_face_detection_vsi_t3(nn_output *pout, face_landmark5_out_t *result)
{
    process_face_detect_aiface((float*)pout->out[0].buf, (float*)pout->out[1].buf, result);
}
//==========================================t3=======================================

//==========================================s5=======================================
#define MAX_FACE_NUM 30000
#define CENTER_FACE_WIDTH 512
#define CENTER_FACE_HEIGHT 288

static float scoreThresh = 0.5;
static float nmsThresh = 0.3;

static int nn_input_width = CENTER_FACE_WIDTH;
static int nn_input_height = CENTER_FACE_HEIGHT;

#define CENTER_FACE_MAX(a, b) ((a > b) ? a : b)
#define CENTER_FACE_MIN(a, b) ((a < b) ? a : b)

static int centerface_nms_comparator(const void *pa, const void *pb)
{
    FaceInfo a = *(FaceInfo *)pa;
    FaceInfo b = *(FaceInfo *)pb;
    float diff = 0;
    diff = b.f32Score - a.f32Score;
    if (diff > 0)
        return 1;
    else if (diff < 0)
        return -1;
    return 0;
}

static void centerface_nms(FaceInfo *boxes, int faceNum, CFaceInfo *filterOutBoxes, int *finalNum)
{
    int tmpFinalNum = *finalNum;

    qsort(boxes, faceNum, sizeof(FaceInfo), centerface_nms_comparator);
    // printf("****************tmp face num: %d\n", faceNum);
    int m = 0, index = 0;
    while (m < faceNum)
    {
        index = m;
        while (index < faceNum)
        {
            if (boxes[index].sort_class == 1)
            {
                CFaceRect cbox = {boxes[index].cBox.f32X1, boxes[index].cBox.f32Y1, boxes[index].cBox.f32X2,
                                  boxes[index].cBox.f32Y2};
                LOGD("final cbox =%f, %f, %f, %f", cbox.f32X1, cbox.f32Y1, cbox.f32X2, cbox.f32Y2);
                CFacePts cpts;
                for (int i = 0; i < 5; i++)
                {
                    cpts.f32X[i] = boxes[index].cPts.f32X[i];
                    cpts.f32Y[i] = boxes[index].cPts.f32Y[i];
                    LOGD("final cpts =%f, %f", cpts.f32X[i], cpts.f32Y[i]);
                }
                LOGD("f32Score=%f", boxes[index].f32Score);
                filterOutBoxes[tmpFinalNum].cBox = cbox;
                filterOutBoxes[tmpFinalNum].cPts = cpts;
                filterOutBoxes[tmpFinalNum].f32Score = boxes[index].f32Score;

                tmpFinalNum++;
                break;
            }
            index++;
            m++;
        }

        for (int i = index + 1; i < faceNum; i++)
        {
            if (boxes[i].sort_class == 1)
            {
                float inter_x1 = CENTER_FACE_MAX(boxes[index].cBox.f32X1, boxes[i].cBox.f32X1);
                float inter_y1 = CENTER_FACE_MAX(boxes[index].cBox.f32Y1, boxes[i].cBox.f32Y1);
                float inter_x2 = CENTER_FACE_MIN(boxes[index].cBox.f32X2, boxes[i].cBox.f32X2);
                float inter_y2 = CENTER_FACE_MIN(boxes[index].cBox.f32Y2, boxes[i].cBox.f32Y2);

                float w = CENTER_FACE_MAX((inter_x2 - inter_x1 + 1), 0.0F);
                float h = CENTER_FACE_MAX((inter_y2 - inter_y1 + 1), 0.0F);

                float inter_area = w * h;
                float area_1 = (boxes[index].cBox.f32X2 - boxes[index].cBox.f32X1 + 1) *
                               (boxes[index].cBox.f32Y2 - boxes[index].cBox.f32Y1 + 1);
                float area_2 = (boxes[i].cBox.f32X2 - boxes[i].cBox.f32X1 + 1) *
                               (boxes[i].cBox.f32Y2 - boxes[i].cBox.f32Y1 + 1);
                float o = inter_area / (area_1 + area_2 - inter_area);
                if (o > nmsThresh)
                    boxes[i].sort_class = 0;
            }
        }
        m++;
    }
    *finalNum = tmpFinalNum;
}

static void decode_i8(int8_t *heatmap, float heatmap_scale, int32_t heat_zero_point,
               int8_t *scale, float scale_scale, int32_t scale_zero_point,
               int8_t *offset, float offset_scale, int32_t offset_zero_point,
               int8_t *landmarks, float landmarks_scale, int32_t land_marks_zero_point,
               FaceInfo *result_decode, int *faceNum, float face_score_threshold, uint32_t* heatmap_size)
{
    int tmpNum = *faceNum;
    int center_face_outw = heatmap_size[2];
    int center_face_outh = heatmap_size[1];

    float temp_score = face_score_threshold / heatmap_scale;

    for (int i = 0; i < center_face_outh; i++)
    {
        for (int j = 0; j < center_face_outw; j++)
        {
            int tempheatmap = heatmap[i * center_face_outw + j];
            tempheatmap = tempheatmap - heat_zero_point;
            if (tempheatmap > temp_score)
            {
                int temp_scale0 = scale[(i * center_face_outw + j) * 2 + 0];
                int temp_scale1 = scale[(i * center_face_outw + j) * 2 + 1];
                temp_scale0 = temp_scale0 - scale_zero_point;
                temp_scale1 = temp_scale1 - scale_zero_point;
                float s0 = exp(scale_scale * temp_scale0) * 4;
                float s1 = exp(scale_scale * temp_scale1) * 4;

                int tempoffset0 = offset[(i * center_face_outw + j) * 2 + 0];
                int tempoffset1 = offset[(i * center_face_outw + j) * 2 + 1];
                tempoffset0 = tempoffset0 - offset_zero_point;
                tempoffset1 = tempoffset1 - offset_zero_point;
                float o0 = offset_scale * tempoffset0;
                float o1 = offset_scale * tempoffset1;

                float x1 = CENTER_FACE_MAX(0., (j + o1 + 0.5) * 4 - s1 / 2);
                float y1 = CENTER_FACE_MAX(0., (i + o0 + 0.5) * 4 - s0 / 2);
                float x2 = 0, y2 = 0;
                x1 = CENTER_FACE_MIN(x1, (float)nn_input_width);
                y1 = CENTER_FACE_MIN(y1, (float)nn_input_height);
                x2 = CENTER_FACE_MIN(x1 + s1, (float)nn_input_width);
                y2 = CENTER_FACE_MIN(y1 + s0, (float)nn_input_height);

                CFaceRect facebox = {x1, y1, x2, y2};
                CFacePts cPts;
                for (int mark = 0; mark < 5; mark++)
                {
                    int tempxmap = landmarks[(i * center_face_outw + j) * 10 + 2 * mark];
                    tempxmap = tempxmap - land_marks_zero_point;
                    int tempymap = landmarks[(i * center_face_outw + j) * 10 + 2 * mark + 1];
                    tempymap = tempymap - land_marks_zero_point;
                    cPts.f32X[mark] = x1 + landmarks_scale * tempymap * s1;
                    cPts.f32Y[mark] = y1 + landmarks_scale * tempxmap * s0;
                }

                result_decode[tmpNum].cBox = facebox;
                result_decode[tmpNum].cPts = cPts;
                result_decode[tmpNum].f32Score = heatmap_scale * tempheatmap;
                result_decode[tmpNum].sort_class = 1;

                tmpNum++;
            }
        }
    }
    *faceNum = tmpNum;
}

static void squareBox(CFaceInfo *OutBoxes, int finalNum)
{
    float w = 0, h = 0, maxSize = 0;
    float cenx, ceny;
    for (int i = 0; i < finalNum; i++)
    {
        w = OutBoxes[i].cBox.f32X2 - OutBoxes[i].cBox.f32X1;
        h = OutBoxes[i].cBox.f32Y2 - OutBoxes[i].cBox.f32Y1;

        maxSize = CENTER_FACE_MAX(w, h);
        cenx = OutBoxes[i].cBox.f32X1 + w / 2;
        ceny = OutBoxes[i].cBox.f32Y1 + h / 2;

        OutBoxes[i].cBox.f32X1 = CENTER_FACE_MAX(cenx - maxSize / 2, 0.f);
        OutBoxes[i].cBox.f32Y1 = CENTER_FACE_MAX(ceny - maxSize / 2, 0.f);
        OutBoxes[i].cBox.f32X2 = CENTER_FACE_MIN(cenx + maxSize / 2, nn_input_width - 1.f);
        OutBoxes[i].cBox.f32Y2 = CENTER_FACE_MIN(ceny + maxSize / 2, nn_input_height - 1.f);
    }
}

static void post_face_detection_adla_s5(nn_output *pout, face_landmark5_out_t *result)
{
    int8_t *heat_map = (int8_t *)pout->out[0].buf;
    int8_t *scale = (int8_t *)pout->out[1].buf;
    int8_t *offset = (int8_t *)pout->out[2].buf;
    int8_t *land_marks = (int8_t *)pout->out[3].buf;

    float heat_scale = pout->out[0].param->quant_data.affine.scale;
    float scale_scale = pout->out[1].param->quant_data.affine.scale;
    float offset_scale = pout->out[2].param->quant_data.affine.scale;
    float land_marks_scale = pout->out[3].param->quant_data.affine.scale;

    int32_t heat_zero_point = (int32_t)pout->out[0].param->quant_data.affine.zeroPoint;
    int32_t scale_zero_point = (int32_t)pout->out[1].param->quant_data.affine.zeroPoint;
    int32_t offset_zero_point = (int32_t)pout->out[2].param->quant_data.affine.zeroPoint;
    int32_t land_marks_zero_point = (int32_t)pout->out[3].param->quant_data.affine.zeroPoint;

    FaceInfo *tmpFaces = (FaceInfo *)malloc(MAX_FACE_NUM * 2 * sizeof(FaceInfo));
    CFaceInfo *cFace = (CFaceInfo *)malloc(MAX_FACE_NUM * sizeof(CFaceInfo));

    int faceNum_decode = 0;
    int faceNum_final = 0;
    float face_score_threshold = scoreThresh;
    uint32_t heatmap_size[4] = {0};
    for (int i = 0; i < 4; i++)
    {
        heatmap_size[i] = pout->out[0].param->sizes[i];
    }

    decode_i8(heat_map, heat_scale, heat_zero_point,
              scale, scale_scale, scale_zero_point,
              offset, offset_scale, offset_zero_point,
              land_marks, land_marks_scale, land_marks_zero_point,
              tmpFaces, &faceNum_decode, face_score_threshold, heatmap_size);

    centerface_nms(tmpFaces, faceNum_decode, cFace, &faceNum_final);

    squareBox(cFace, faceNum_final);

    result->detNum = faceNum_final;
    for (int i = 0; i < result->detNum; i++)
    {
        result->facebox[i].score = cFace[i].f32Score;
        result->facebox[i].x = cFace[i].cBox.f32X1;
        result->facebox[i].y = cFace[i].cBox.f32Y1;
        result->facebox[i].w = cFace[i].cBox.f32X2 - cFace[i].cBox.f32X1;
        result->facebox[i].h = cFace[i].cBox.f32Y2 - cFace[i].cBox.f32Y1;
    }

    if (tmpFaces != NULL)
    {
        free(tmpFaces);
    }
    if (cFace != NULL)
    {
        free(cFace);
    }
}
//==========================================s5=======================================

// #define BILLION 1000000000
// static uint64_t tmsStart,tmsEnd, msVal, usVal, sigStart, sigEnd;
// static uint64_t get_perf_count()
// {
//     struct timespec ts;
//     clock_gettime(CLOCK_MONOTONIC, &ts);
//     return (uint64_t)((uint64_t)ts.tv_nsec + (uint64_t)ts.tv_sec * BILLION);
// }

// #include <android/log.h>
// #define LOG_TAG "NN_SDK:"

void *aml_face_detection_output_get(void *qcontext, aml_output_config_t outconfig, face_landmark5_out_t *face_landmark5_result)
{
    aml_platform_info_t platform_info;
    memset(&platform_info, 0, sizeof(platform_info));
    aml_read_chip_info(&platform_info);

    nn_output *outdata = NULL;

    switch (platform_info.hw_type)
    {
        case AML_HARDWARE_VSI_UNIFY:
            outconfig.format = AML_OUTDATA_FLOAT32;
            outdata = (nn_output *)aml_module_output_get(qcontext, outconfig);
            post_face_detection_vsi_t3(outdata, face_landmark5_result);
            break;
        case AML_HARDWARE_ADLA:
            // outconfig.format = AML_OUTDATA_RAW;
            outconfig.format = AML_OUTDATA_DMA;

            // tmsStart = get_perf_count();
            outdata = (nn_output *)aml_module_output_get(qcontext, outconfig);
            // tmsEnd = get_perf_count();
	        // msVal = (tmsEnd-tmsStart)/1000000;
	        // usVal = (tmsEnd-tmsStart)/1000;
            // printf("AML_PERF_OUTPUT_GET = %lldms or %lldus \n",msVal,usVal);

            // __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "xinxin_01");
            // tmsStart = get_perf_count();
            post_face_detection_adla_s5(outdata, face_landmark5_result);
            // tmsEnd = get_perf_count();
	        // msVal = (tmsEnd-tmsStart)/1000000;
	        // usVal = (tmsEnd-tmsStart)/1000;
            // printf("POSTPROCESS = %lldms or %lldus \n",msVal,usVal);
            // __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "xinxin_02");
            break;
        default:
            break;
    }

    return NULL;
}