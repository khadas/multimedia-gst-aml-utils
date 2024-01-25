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
#define LOG_TAG "cv_postprocess"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nn_sdk.h"
#include "nn_util.h"
#include "cv_postprocess.h"

#ifndef ANDROID_SDK
#include <set>
#include <iostream>
#endif

#ifdef USE_OPENCV
#include<opencv2/dnn.hpp>
#include<opencv2/imgproc.hpp>
#include<opencv2/highgui.hpp>
using namespace cv;
using namespace cv::dnn;
#endif

const char *sdk_demo_version = "NNSDK_DEMO,v2.3.0,2023.03";

extern "C"
void *post_process_all_module(aml_module_t type,nn_output *pOut)
{
    //LOGI("sdk_demo_version is %s\n",sdk_demo_version);
	aml_module_t nettype = type;
	void *data = NULL;

	switch (nettype)
	{
	case IMAGE_CLASSIFY:
		data  = postprocess_classify(pOut);
		break;
	case OBJECT_DETECT:
		data  = postprocess_object_detect(pOut);
		break;
	case FACE_DETECTION:
		data = postprocess_facedet(pOut);
		break;
	case FACE_LANDMARK_5:
		data = postprocess_faceland5(pOut);
		break;
	case FACE_LANDMARK_68:
		data = postprocess_faceland68(pOut);
		break;
	case FACE_RECOGNIZE:
		data = postprocess_facereg(pOut);
		break;
	case FACE_COMPARISON:
		data = postprocess_facecompare(pOut);
		break;
	case FACE_AGE:
		data = postprocess_age(pOut);
		break;
	case FACE_GENDER:
		data = postprocess_gender(pOut);
		break;
	case FACE_EMOTION:
		data = postprocess_emotion(pOut);
		break;
	case BODY_POSE:
	#ifdef USE_OPENCV
		data = postprocess_bodypose(pOut);
	#endif
		break;
	case FINGER_POSE:
		break;
	case HEAD_DETECTION:
		data = postprocess_headdet(pOut);
		break;
	case CARPLATE_DETECTION:
		data = postprocess_plate_detect(pOut);
		break;
	case CARPLATE_RECOG:
		data = postprocess_plate_recognize(pOut);
		break;
	case TEXT_DETECTION:
	#ifdef USE_OPENCV
		data = postprocess_textdet(pOut);
	#endif
		break;
	case IMAGE_SR:
		break;
	case IMAGE_SEGMENTATION:
		data = postprocess_segmentation(pOut);
		break;
	case PERSON_DETECT:
		data = postprocess_person_detect(pOut);
		break;
	case YOLOFACE_V2:
		data = postprocess_yoloface_v2(pOut);
		break;
	case YOLO_V2:
		data = postprocess_object_detect(pOut);
		break;
	case YOLO_V3:
		data = postprocess_yolov3(pOut);
		break;
	case FACE_NET:
		data = postprocess_facenet(pOut);
		break;
	case FACE_RECOG_U:
		data = postprocess_facereg_uint(pOut);
		break;
    case FACE_RFB_DETECTION:
        data = postprocess_rfb_facedet(pOut);
        break;
    case AML_PERSON_DETECT:
	data = postprocess_aml_person_detect(pOut);
        break;
	default:
		break;
	}
	return data;
}
