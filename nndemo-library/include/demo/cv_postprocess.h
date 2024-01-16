/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * aml_nnsdk
 */

#ifndef _AMLOGIC_NN_SDK_POSTPROCESS_H
#define _AMLOGIC_NN_SDK_POSTPROCESS_H
#include "nn_demo.h"
#ifdef __cplusplus
extern "C" {
#endif
extern float bbox_32[12][20][8]; //384/32,640/32
extern float bbox_16[24][40][8];
extern float bbox_8[48][80][8];
//prob score,input
extern float prob_32[240][2][2];
extern float prob_16[960][2][2];
extern float prob_8[3840][2][2];
//land mark
extern float land_32[12][20][20]; //384/32,640/32
extern float land_16[24][40][20];
extern float land_8[48][80][20];

extern float p_bbox_32[8][12][20]; //384/32,640/32
extern float p_bbox_16[8][24][40];
extern float p_bbox_8[8][48][80];
//prob score,input
extern float p_prob_32[480][2];
extern float p_prob_16[1920][2];
extern float p_prob_8[7680][2];

extern float bbox[5875][4];
extern float pprob[5875][2];
extern float llandmark[5875][10];

typedef struct detection_{
    box bbox;
    float *prob;
    float objectness;
    int classes;
    int sort_class;
}detection;

void* post_process_all_module(aml_module_t type,nn_output *pOut);
void* post_textdet(float* score,float*geo);
void* post_posenet(float* pbody,unsigned int size);
int post_facedet(face_detect_out_t* pface_det_result);
int post_faceland5(face_landmark5_out_t* pface_landmark5_result);
int face_detect_postprocess(face_detect_out_t* pface_det_result);
int face_landmark5_postprocess(face_landmark5_out_t* pface_landmark5_result);
void* post_obj_dect(float *predictions, int width, int height, int modelWidth, int modelHeight, int input_num);
void* object_detect_postprocess(float *predictions, int width, int height, int modelWidth, int modelHeight, int input_num);
void* yolov3_postprocess_multi_thread(nn_output *nn_out);
void* yolov3_postprocess(float **predictions, int width, int height, int modelWidth, int modelHeight, int input_num);
void* yoloface_detect_postprocess(float *predictions, int width, int height, int modelWidth, int modelHeight, int input_num);
void process_top5(float *buf,unsigned int num,img_classify_out_t* clsout);
int post_facedet(face_detect_out_t* pface_det_result);
int person_do_post_process(person_detect_out_t* pperson_detect_result);
float retina_box_iou(box a, box b);
void do_global_sort(box *boxe1,box *boxe2, float prob1[][1],float prob2[][1], int len1,int len2,float thresh);
void *post_process_all_module(aml_module_t type,nn_output *pOut);
int face_rfb_detect_postprocess(face_rfb_detect_out_t* pface_rfb_det_result);
void* postprocess_aml_person_detect(nn_output *pout);
void* postprocess_plate_detect(nn_output *pout);
void* postprocess_plate_recognize(nn_output *pOut);


/************      classify_postprocess.c        *******/
void* postprocess_classify(nn_output *pout);
void* postprocess_facecompare(nn_output *pout);
void* postprocess_facereg(nn_output *pout);
void* postprocess_facereg_uint(nn_output *pout);
void* postprocess_facenet(nn_output *pout);
void* postprocess_rfb_facedet(nn_output *pout);
void* postprocess_faceland68(nn_output *pout);
void* postprocess_faceland5(nn_output *pout);
void* postprocess_facedet(nn_output *pout);

void* postprocess_carreg(nn_output *pout);

/************    face_attribute_postprocess.c    ********/
void* postprocess_gender(nn_output *pout);
void* postprocess_age(nn_output *pout);
void* postprocess_emotion(nn_output *pout);

/************    head_detect_postprocess.c    ********/
void* postprocess_headdet(nn_output *pout);

/*************      pose_postprocess.c      *******/
void* post_posenet(float* pbody,unsigned int size);
/**************   segmentation_postprocess.c    **********/
void* postprocess_segmentation(nn_output *pout);

void* postprocess_person_detect(nn_output *pout);
void* postprocess_yoloface_v2(nn_output *pout);
void* postprocess_yolov2(nn_output *pout);
void* postprocess_yolov3(nn_output *pout);
void* postprocess_object_detect(nn_output *pout);
void data_to_fp32(float *new_buffer, unsigned char *old_buffer, int sz, float scale, int zero_point, int type);




#ifdef __cplusplus
} //extern "C"
#endif
#endif