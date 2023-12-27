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

#include <pthread.h>
#include <unistd.h>       // for syscall()
#include <sys/syscall.h>  // for SYS_xxx definitions

// for test performance time
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>


static struct timeval g_start = {0};
static struct timeval g_end = {0};
static double g_timecost = 0.0f;



// for multiple thread
static int yolov3_box_num_after_filter_arr[3] = {0};

extern int g_detect_number;
box get_region_box(float *x, float *biases, int n, int index, int i, int j, int w, int h)
{
    box b;

    b.x = (i + logistic_activate(x[index + 0])) / w;
    b.y = (j + logistic_activate(x[index + 1])) / h;
    b.w = exp(x[index + 2]) * biases[2*n]   / w;
    b.h = exp(x[index + 3]) * biases[2*n+1] / h;
    return b;
}

int max_index(float *a, int n)
{
	int i, max_i = 0;
    float max = a[0];

    if (n <= 0)
		return -1;

    for (i = 1; i < n; ++i)
	{
        if (a[i] > max)
		{
            max = a[i];
            max_i = i;
        }
    }
    return max_i;
}

float colors[6][3] = { {1,0,1}, {0,0,1},{0,1,1},{0,1,0},{1,1,0},{1,0,0} };

float get_color(int c, int x, int max)
{
    float ratio = ((float)x/max)*5;
    int i = floor(ratio);
    int j = ceil(ratio);
	float r = 0;
    ratio -= i;
    r = (1-ratio) * colors[i][c] + ratio*colors[j][c];
    return r;
}

obj_detect_out_t dectout ;
void* yolov2_result(int num, float thresh, box *boxes, float **probs, int classes)
{
    int i=0,detect_num = 0;

	if (dectout.pBox == NULL)
	{
		dectout.pBox = (detBox*)malloc(MAX_DETECT_NUM*sizeof(detBox));
	}
	if (dectout.pBox == NULL)
	{
		return NULL;
	}
    for (i = 0; i < num; ++i)
	{
        int classId = max_index(probs[i], classes);
        float prob = probs[i][classId];
        if (prob > thresh)
		{
			if (detect_num >= g_detect_number)
			{
				break;
			}
			dectout.pBox[detect_num].x = boxes[i].x;
			dectout.pBox[detect_num].y = boxes[i].y;
			dectout.pBox[detect_num].w = boxes[i].w;
			dectout.pBox[detect_num].h = boxes[i].h;
			dectout.pBox[detect_num].score = prob;
			dectout.pBox[detect_num].objectClass = (float)classId;
			detect_num++ ;
		}
	}
	dectout.detNum = detect_num;
	return (void*)&dectout;
}

int yolo_v3_post_process_onescale(float *predictions, int input_size[3] , float *biases, box *boxes, float **pprobs, float threshold_in, int *yolov3_box_num_after_filter)
{
    int i,j;
    int num_class = 80;
    int coords = 4;
    int bb_size = coords + num_class + 1;//85
    int num_box = input_size[2]/bb_size;//3=85*3/85
    int modelWidth = input_size[0];//13 26 52
    int modelHeight = input_size[1];
    float threshold=threshold_in;

	////////////////////////////////////////////////////////////////////////////////////////////////
	// [Guoping]remove the allocate in for loop, pprobs will be allocated successful outside
    // for (j = 0; j < modelWidth*modelHeight*num_box; ++j){
    //     pprobs[j] = (float *)calloc(num_class+1, sizeof(float *));
    // }
	////////////////////////////////////////////////////////////////////////////////////////////////

    int ck0, batch = 1;
    flatten(predictions, modelWidth*modelHeight, bb_size*num_box, batch, 1);

    for (i = 0; i < modelHeight*modelWidth*num_box; ++i)
    {
        for (ck0=coords;ck0<bb_size;ck0++ )
        {
            int index = bb_size*i;

            predictions[index + ck0] = logistic_activate(predictions[index + ck0]);
            if (ck0 == coords)
            {
                if (predictions[index+ck0] <= threshold)
                {
                    break;
                }
            }
        }
    }

    for (i = 0; i < modelWidth*modelHeight; ++i)
    {
        int row = i / modelWidth;
        int col = i % modelWidth;
        int n =0;
        for (n = 0; n < num_box; ++n)
        {
            int index = i*num_box + n;
            int p_index = index * bb_size + 4;
            float scale = predictions[p_index];
            int box_index = index * bb_size;
            int class_index = 0;
            class_index = index * bb_size + 5;

            if (scale>threshold)
            {
				(*yolov3_box_num_after_filter)++;
                for (j = 0; j < num_class; ++j)
                {
                    float prob = scale*predictions[class_index+j];
                    pprobs[index][j] = (prob > threshold) ? prob : 0;
                }
                boxes[index] = get_region_box(predictions, biases, n, box_index, col, row, modelWidth, modelHeight);
            }
            boxes[index].prob_obj = (scale>threshold)?scale:0;
        }
    }
    return 0;
}

void* object_detect_postprocess(float *predictions, int width, int height, int modelWidth, int modelHeight, int input_num)
{
	int i,j,n;
	float threshold = 0.24;
	float iou_threshold = 0.4;
	int num_class = 80;
	int num_box = 5;
	int grid_size = 13;
	float biases[10] = {0.738768,0.874946,2.422040,2.657040,4.309710,7.044930,10.246000,4.594280,12.686800,11.874100};
	void* objout = NULL;

	box *boxes = (box *)calloc(modelWidth*modelHeight*num_box, sizeof(box));
    float **probs = (float **)calloc(modelWidth*modelHeight*num_box, sizeof(float *));

	for (j = 0; j < modelWidth*modelHeight*num_box; ++j)
	{
		probs[j] = (float *)calloc(num_class+1, sizeof(float *));// calloc "num_class+1" float for every W*H*num_box
	}

	{
		int i,b;
		int coords = 4,classes = 80;
		int size = coords + classes + 1;
		int w = 13;
		int h = 13;
		int n = 425/size;
	    int batch = 1;
		flatten(predictions, w*h, size*n, batch, 1);

		for (b = 0; b < batch; ++b)
		{
			for (i = 0; i < h*w*n; ++i)
			{
				int index = size*i + b*input_num;
				predictions[index + 4] = logistic_activate(predictions[index + 4]);
			}
		}

		for (b = 0; b < batch; ++b)
		{
			for (i = 0; i < h*w*n; ++i)
			{
				int index = size*i + b*input_num;
				softmax(predictions + index + 5, classes, 1, predictions + index + 5);
			}
		}
	}

	for (i = 0; i < modelWidth*modelHeight; ++i)
	{
		int row = i / modelWidth;
		int col = i % modelWidth;
		for (n = 0; n < num_box; ++n)
		{
			int index = i*num_box + n;
			int p_index = index * (num_class + 5) + 4;
			float scale = predictions[p_index];
			int box_index = index * (num_class + 5);
			int class_index = 0;
			boxes[index] = get_region_box(predictions, biases, n, box_index, col, row, modelWidth, modelHeight);
			class_index = index * (num_class + 5) + 5;
			for (j = 0; j < num_class; ++j)
			{
				float prob = scale*predictions[class_index+j];
				probs[index][j] = (prob > threshold) ? prob : 0;
			}
		}
	}

	do_nms_sort(boxes, probs, grid_size*grid_size*num_box, num_class, iou_threshold);
	objout = yolov2_result(grid_size*grid_size*num_box, threshold, boxes, probs, num_class);

	free(boxes);
	boxes = NULL;

	for (j = 0; j < grid_size*grid_size*num_box; ++j) {
		free(probs[j]);
		probs[j] = NULL;
	}

	free(probs);
	probs = NULL;
	return objout;
}


typedef struct _yolov3_param
{
	// in and out data
	nn_output *out_data;
	nn_output *in_data;

	// scale index
	int index;

	int size[3];

	box *boxes;
    float **pprobs;

	int box1;
}yolov3_param;


static void *yolov3_postprocess_threadfunc(void *arg)
{
    // struct timeval start;
    // struct timeval end;
    // double time_total;
    // gettimeofday(&start, NULL);

	int id = syscall(SYS_gettid);
    yolov3_param *pParam = (yolov3_param *)arg;
	nn_output *out_data 		= pParam->out_data;
	nn_output *in_data 	= pParam->in_data;

	box *boxes = pParam->boxes;
	float **pprobs = pParam->pprobs;
	int box1 = pParam->box1;

	if (out_data != in_data)
	{
		// convert int8 to float32
		pp_nchw_f32_onescale(out_data, in_data, pParam->index);
	}

	float biases[18] = {10/8., 13/8., 16/8., 30/8., 33/8., 23/8., 30/16., 61/16., 62/16., 45/16., 59/16., 119/16., 116/32., 90/32., 156/32., 198/32., 373/32., 326/32.};
    float threshold = 0.4;

	float *yolov3_buffer = NULL;
	yolov3_buffer = (float*)out_data->out[pParam->index].buf;

	// process one scale
	yolo_v3_post_process_onescale(yolov3_buffer, pParam->size, &biases[(pParam->index)*6], &boxes[box1], &pprobs[box1], threshold, &yolov3_box_num_after_filter_arr[pParam->index]);

	// gettimeofday(&end, NULL);
	// time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
	// start = end;
	//printf("yolov3_postprocess_threadfunc[%d] done, time=%lf uS \n", pParam->index, time_total);

    return NULL;
}
static void* data_to_fp32_post_process(nn_output *pout,float **fp32_buffer){
    //float *fp32_buffer[3] = {NULL};
    int size  = 0;
    float scale = 0.0;
    int zp = 0;
    int data_format;

    for (int i = 0; i < pout->num; i++)
    {
        size = pout->out[i].size;
        data_format = pout->out[i].param->data_format;
        scale = pout->out[i].param->quant_data.affine.scale;
        zp = pout->out[i].param->quant_data.affine.zeroPoint;
        //printf("size: %d, data_format: %d, scale: %f, zp: %d\n", size, data_format, scale, zp);

        fp32_buffer[i] = (float *)malloc(size * sizeof(float));//free
        data_to_fp32(fp32_buffer[i], pout->out[i].buf, size, scale, zp, data_format);
    }
    return NULL;
}

// [Guoping] More improve point: (time cost is in T7C)
// add init/de-init function, move boxes/pprobs/temp_boxes/temp_probs allcation to init function. reuse memory
void* yolov3_postprocess_multi_thread(nn_output *nn_out)
{
    struct timeval start;
    struct timeval end;
    double time_total;
    gettimeofday(&start, NULL);

    unsigned int i = 0;
    int8_t *p = NULL;
    unsigned int buf_size = 0;
    nn_output *nn_out_tmp = NULL;  // float
	nn_output *pout = NULL;

    aml_platform_info_t platform_info;
    memset(&platform_info, 0, sizeof(platform_info));
    aml_read_chip_info(&platform_info);

    float *yolov3_buffer[3] = {NULL};

    if (platform_info.hw_type == AML_HARDWARE_VSI_UNIFY)
    {
        data_to_fp32_post_process(nn_out,yolov3_buffer);

        // needn't convert, just switch the buffer
		//float *yolov3_buffer[3] = {NULL};
        // yolov3_buffer[0] = (float*)nn_out->out[2].buf;
        // yolov3_buffer[1] = (float*)nn_out->out[1].buf;
        // yolov3_buffer[2] = (float*)nn_out->out[0].buf;
		// nn_out->out[0].buf = yolov3_buffer[0];
		// nn_out->out[1].buf = yolov3_buffer[1];
		// nn_out->out[2].buf = yolov3_buffer[2];

        nn_out->out[0].buf = yolov3_buffer[2];
		nn_out->out[1].buf = yolov3_buffer[1];
		nn_out->out[2].buf = yolov3_buffer[0];

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

		gettimeofday(&end, NULL);
		time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
		start = end;
		//printf("nn_out_tmp malloc and copy, time=%lf uS \n", time_total);

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
        pout = nn_out_tmp;
	}

	int nn_width,nn_height, nn_channel;
	void* objout = NULL;
    nn_width = 416;
    nn_height = 416;
    nn_channel = 3;
    (void)nn_channel;
    int size[3]={nn_width/32, nn_height/32, 85*3};//13 13 85*3

    int j, k, index;
    int num_class = 80;
    float threshold = 0.4;
    float iou_threshold = 0.4;

    float biases[18] = {10/8., 13/8., 16/8., 30/8., 33/8., 23/8., 30/16., 61/16., 62/16., 45/16., 59/16., 119/16., 116/32., 90/32., 156/32., 198/32., 373/32., 326/32.};
    int size2[3] = {size[0]*2,size[1]*2,size[2]};//26 26 85*3
    int size4[3] = {size[0]*4,size[1]*4,size[2]};//52 52 85*3
    int len1 = size[0]*size[1]*size[2];//43095=13*13*85*3
    int box1 = len1/(num_class+5);//507=13*13*3

    /////////////////////////////////////////////////////////////////
    box *boxes = (box *)calloc(box1*(1+4+16), sizeof(box));//13*13*3 + 13*2*13*2**3 + 13*4*13*4*3
    float **pprobs = (float **)calloc(box1*(1+4+16), sizeof(float **));

	////////////////////////////////////////////////////////////////////////////////////////////////
	// [Guoping] allocate a big memory, and separate them to 3 layers
    int coords = 4;
    int bb_size = coords + num_class + 1;//85

	// calculate the total probs count
	int num_box = size[2]/bb_size;
	int layer_probs_cnt = size[0]*size[1]*num_box;
	int probs_cnt = layer_probs_cnt * (num_class+1);

	num_box = size2[2]/bb_size;
	int layer2_probs_cnt = size2[0]*size2[1]*num_box;
	probs_cnt += layer2_probs_cnt * (num_class+1);

	num_box = size4[2]/bb_size;
	int layer4_probs_cnt = size4[0]*size4[1]*num_box;
	probs_cnt += layer4_probs_cnt * (num_class+1);

	// allocate the probs in an array
	float *probs = (float *)calloc(probs_cnt, sizeof(float *));

	// scale 1
    for (j = 0; j < layer_probs_cnt; ++j) {
        pprobs[j] = (float **)(void *)&probs[(num_class+1)*j];
    }
	// scale 2
    for (j = 0; j < layer2_probs_cnt; ++j) {
        pprobs[box1+j] = (float **)(void *)&probs[layer_probs_cnt+(num_class+1)*j];
    }
	// scale 4
    for (j = 0; j < layer4_probs_cnt; ++j) {
        pprobs[box1*(1+4)+j] = (float **)(void *)&probs[layer_probs_cnt+layer2_probs_cnt+(num_class+1)*j];
    }
	////////////////////////////////////////////////////////////////////////////////////////////////

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    //printf("probs_cnt=%d, calloc, time=%lf uS \n", probs_cnt, time_total);

	int err = 0;
	pthread_t t_thread[3];
	yolov3_param *pParam = malloc(sizeof(yolov3_param) * 3);

	int box1_array[3] = {5, 1, 0};
	int *size_array[3] = {size4, size2, size};

	yolov3_box_num_after_filter_arr[0] = 0;
	yolov3_box_num_after_filter_arr[1] = 0;
	yolov3_box_num_after_filter_arr[2] = 0;

	// scale 1/2 put to another thread
	for (i = 1; i < 3; i++)
    {
		// scaler 0, is the slowest
		pParam[i].in_data = nn_out;
		pParam[i].out_data = pout;
		pParam[i].index = i;
		memcpy (pParam[i].size, size_array[i], sizeof(size));
		pParam[i].boxes = boxes;
		pParam[i].pprobs = pprobs;
		pParam[i].box1 = box1 * box1_array[i];

		pthread_create(&t_thread[i], NULL, &yolov3_postprocess_threadfunc, (void *)&pParam[i]);
    }

	// scaler 0 is most Complex, do by this thread
	i = 0;
	pParam[i].in_data = nn_out;
	pParam[i].out_data = pout;
	pParam[i].index = i;
	memcpy (pParam[i].size, size_array[i], sizeof(size));
	pParam[i].boxes = boxes;
	pParam[i].pprobs = pprobs;
	pParam[i].box1 = box1 * box1_array[i];
	yolov3_postprocess_threadfunc(&pParam[i]);

	// wait other thread complete
	for (i = 1; i < 3; i++)
    {
		pthread_join(t_thread[i], NULL);
    }

    //zyadd
    for (int i = 0; i < pout->num; i++)
    {
        if (yolov3_buffer[i])
        {
            free(yolov3_buffer[i]);
            yolov3_buffer[i] = NULL;
            //printf("free yolov3_buffer\n");
        }
    }

	if (NULL != pParam)
	{
		free(pParam);
		pParam = NULL;
	}

	if (NULL != nn_out_tmp)
	{
		free(nn_out_tmp);
		nn_out_tmp = NULL;
	}

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    printf("All yolov3_postprocess_threadfunc done, time=%lf uS \n", time_total);

	int yolov3_box_num_after_filter = yolov3_box_num_after_filter_arr[0];
	yolov3_box_num_after_filter += yolov3_box_num_after_filter_arr[1];
	yolov3_box_num_after_filter += yolov3_box_num_after_filter_arr[2];
	//printf("yolov3_box_num_after_filter=%d \n", yolov3_box_num_after_filter);

	box *tmp_boxes = (box *)calloc(yolov3_box_num_after_filter, sizeof(box));
	float **tmp_pprobs = (float **)calloc(yolov3_box_num_after_filter, sizeof(float *));

	for (index = 0, k = 0; index < box1*(1+4+16); index++)
	{
		//printf("[index:%d]fabs 1 \n", index);
		if ((fabs(boxes[index].prob_obj)-0) > 0.000001)
		{
			//printf("[k:%d]fabs 2 \n", k);
			tmp_pprobs[k] = pprobs[index];
			tmp_boxes[k] = boxes[index];
			k++;
		}
	}

	do_nms_sort(tmp_boxes, tmp_pprobs, yolov3_box_num_after_filter, num_class, iou_threshold);
	objout = yolov2_result(yolov3_box_num_after_filter, threshold, tmp_boxes, tmp_pprobs, num_class);

	free(tmp_boxes);
	tmp_boxes = NULL;
	free(tmp_pprobs);
	tmp_pprobs = NULL;

	////////////////////////////////////////////////////////////////////////////////////////////////
	// [Guoping] remove free in for loop
    // for (j = 0; j < box1*(1+4+16); ++j)
    // {
    //     free(probs[j]);
    //     probs[j] = NULL;
    // }

    free(probs);
    probs = NULL;
	////////////////////////////////////////////////////////////////////////////////////////////////

    free(boxes);
    boxes = NULL;
    free(pprobs);
    pprobs = NULL;

	gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    //printf("yolov2_result and free, time=%lf uS \n", time_total);

    return objout;
}
//////////////////////////////////////////////////////////////////////////////////////////////


// [Guoping] More improve point: (time cost is in T7C)
// add init/de-init function, move boxes/pprobs/temp_boxes/temp_probs allcation to init function. reuse memory, can reduce about 1ms
// can move scale 3 to another thread, can reduce about 2ms
void* yolov3_postprocess(float **predictions, int width, int height, int modelWidth, int modelHeight, int input_num)
{
    struct timeval start;
    struct timeval end;
    double time_total;
    gettimeofday(&start, NULL);

	int nn_width,nn_height, nn_channel;
	void* objout = NULL;
    nn_width = 416;
    nn_height = 416;
    nn_channel = 3;
    (void)nn_channel;
    int size[3]={nn_width/32, nn_height/32,85*3};//13 13 85*3

    int j, k, index;
    int num_class = 80;
    float threshold = 0.4;
    float iou_threshold = 0.4;

    float biases[18] = {10/8., 13/8., 16/8., 30/8., 33/8., 23/8., 30/16., 61/16., 62/16., 45/16., 59/16., 119/16., 116/32., 90/32., 156/32., 198/32., 373/32., 326/32.};
    int size2[3] = {size[0]*2,size[1]*2,size[2]};//26 26 85*3
    int size4[3] = {size[0]*4,size[1]*4,size[2]};//52 52 85*3
    int len1 = size[0]*size[1]*size[2];//43095=13*13*85*3
    int box1 = len1/(num_class+5);//507=13*13*3

    box *boxes = (box *)calloc(box1*(1+4+16), sizeof(box));//13*13*3 + 13*2*13*2**3 + 13*4*13*4*3
    float **pprobs = (float **)calloc(box1*(1+4+16), sizeof(float **));

	////////////////////////////////////////////////////////////////////////////////////////////////
	// [Guoping] allocate a big memory, and separate them to 3 layers
    int coords = 4;
    int bb_size = coords + num_class + 1;//85

	// calculate the total probs count
	int num_box = size[2]/bb_size;
	int layer_probs_cnt = size[0]*size[1]*num_box;
	int probs_cnt = layer_probs_cnt * (num_class+1);

	num_box = size2[2]/bb_size;
	int layer2_probs_cnt = size2[0]*size2[1]*num_box;
	probs_cnt += layer2_probs_cnt * (num_class+1);

	num_box = size4[2]/bb_size;
	int layer4_probs_cnt = size4[0]*size4[1]*num_box;
	probs_cnt += layer4_probs_cnt * (num_class+1);

	// allocate the probs in an array
	float *probs = (float *)calloc(probs_cnt, sizeof(float *));

	// scale 1
    for (j = 0; j < layer_probs_cnt; ++j) {
        pprobs[j] = (float **)(void *)&probs[(num_class+1)*j];
    }
	// scale 2
    for (j = 0; j < layer2_probs_cnt; ++j) {
        pprobs[box1+j] = (float **)(void *)&probs[layer_probs_cnt+(num_class+1)*j];
    }
	// scale 4
    for (j = 0; j < layer4_probs_cnt; ++j) {
        pprobs[box1*(1+4)+j] = (float **)(void *)&probs[layer_probs_cnt+layer2_probs_cnt+(num_class+1)*j];
    }
	////////////////////////////////////////////////////////////////////////////////////////////////

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    printf("probs_cnt=%d, calloc, time=%lf uS \n", probs_cnt, time_total);

    yolo_v3_post_process_onescale(predictions[2], size, &biases[12], boxes, &pprobs[0], threshold, &yolov3_box_num_after_filter_arr[2]); //final layer

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    printf("yolo_v3_post_process_onescale(predictions[2], time=%lf uS \n", time_total);

	yolo_v3_post_process_onescale(predictions[1], size2, &biases[6], &boxes[box1], &pprobs[box1], threshold, &yolov3_box_num_after_filter_arr[1]);

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    printf("yolo_v3_post_process_onescale(predictions[1], time=%lf uS \n", time_total);

	yolo_v3_post_process_onescale(predictions[0], size4, &biases[0],  &boxes[box1*(1+4)], &pprobs[box1*(1+4)], threshold, &yolov3_box_num_after_filter_arr[0]);

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    printf("yolo_v3_post_process_onescale(predictions[0], time=%lf uS \n", time_total);

	int yolov3_box_num_after_filter = yolov3_box_num_after_filter_arr[0];
	yolov3_box_num_after_filter += yolov3_box_num_after_filter_arr[1];
	yolov3_box_num_after_filter += yolov3_box_num_after_filter_arr[2];
	printf("yolov3_box_num_after_filter=%d \n", yolov3_box_num_after_filter);

	box *tmp_boxes = (box *)calloc(yolov3_box_num_after_filter, sizeof(box));
	float **tmp_pprobs = (float **)calloc(yolov3_box_num_after_filter, sizeof(float *));

	for (index = 0, k = 0; index < box1*(1+4+16); index++)
	{
		if ((fabs(boxes[index].prob_obj)-0) > 0.000001)
		{
			tmp_pprobs[k] = pprobs[index];
			tmp_boxes[k] = boxes[index];
			k++;
		}
	}

	do_nms_sort(tmp_boxes, tmp_pprobs, yolov3_box_num_after_filter, num_class, iou_threshold);
	objout = yolov2_result(yolov3_box_num_after_filter, threshold, tmp_boxes, tmp_pprobs, num_class);

	free(tmp_boxes);
	tmp_boxes = NULL;
	free(tmp_pprobs);
	tmp_pprobs = NULL;

	////////////////////////////////////////////////////////////////////////////////////////////////
	// [Guoping] remove free in for loop
    // for (j = 0; j < box1*(1+4+16); ++j)
    // {
    //     free(probs[j]);
    //     probs[j] = NULL;
    // }

    free(probs);
    probs = NULL;
	////////////////////////////////////////////////////////////////////////////////////////////////

    free(boxes);
    boxes = NULL;
    free(pprobs);
    pprobs = NULL;

	gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    printf("yolov2_result and free, time=%lf uS \n", time_total);

    return objout;
}

void* yoloface_detect_postprocess(float *predictions, int width, int height, int modelWidth, int modelHeight, int input_num)
{
	int i,j,n;
	float threshold = 0.24;
	float iou_threshold = 0.4;
	int num_class = 1;
	int num_box = 5;
	int grid_size = 13;
	float biases[10] = {1.08,1.19,  3.42,4.41,  6.63,11.38,  9.42,5.11,  16.62,10.52};
	void* objout = NULL;

	box *boxes = (box *)calloc(modelWidth*modelHeight*num_box, sizeof(box));
    float **probs = (float **)calloc(modelWidth*modelHeight*num_box, sizeof(float *));

	for (j = 0; j < modelWidth*modelHeight*num_box; ++j)
	{
		probs[j] = (float *)calloc(num_class+1, sizeof(float *));// calloc "num_class+1" float for every W*H*num_box
	}
	{
		int i,b;
		int coords = 4,classes = 1;
		int size = coords + classes + 1;
		int w = 13;
		int h = 13;
		int n = 5;
	    int batch = 1;

		flatten(predictions, w*h, size*n, batch, 1);

		for (b = 0; b < batch; ++b)
		{
			for (i = 0; i < h*w*n; ++i)
			{
				int index = size*i + b*input_num;
				predictions[index + 4] = logistic_activate(predictions[index + 4]);
			}
		}
		for (b = 0; b < batch; ++b)
		{
			for (i = 0; i < h*w*n; ++i)
			{
				int index = size*i + b*input_num;
				softmax(predictions + index + 5, classes, 1, predictions + index + 5);
			}
		}
	}

	for (i = 0; i < modelWidth*modelHeight; ++i)
	{
		int row = i / modelWidth;
		int col = i % modelWidth;
		for (n = 0; n < num_box; ++n)
		{
			int index = i*num_box + n;
			int p_index = index * (num_class + 5) + 4;
			float scale = predictions[p_index];
			int box_index = index * (num_class + 5);
			int class_index = 0;
			boxes[index] = get_region_box(predictions, biases, n, box_index, col, row, modelWidth, modelHeight);
			class_index = index * (num_class + 5) + 5;
			for (j = 0; j < num_class; ++j)
			{
				float prob = scale*predictions[class_index+j];
				probs[index][j] = (prob > threshold) ? prob : 0;
			}
		}
	}

	do_nms_sort(boxes, probs, grid_size*grid_size*num_box, num_class, iou_threshold);
	objout = yolov2_result(grid_size*grid_size*num_box, threshold, boxes, probs, num_class);

	free(boxes);
	boxes = NULL;

	for (j = 0; j < grid_size*grid_size*num_box; ++j) {
		free(probs[j]);
		probs[j] = NULL;
	}

	free(probs);
	probs = NULL;

	return objout;
}
