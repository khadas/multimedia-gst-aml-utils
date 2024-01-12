#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <fstream>

#define USE_DMA_BUFFER

#include <math.h>
#include <float.h>
#include <unistd.h>
using namespace std;

#include "nn_sdk.h"
#include "nn_demo.h"
#include "nn_detect.h"
#include "nn_detect_utils.h"
#include "detect_log.h"


// for test performance time
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>



static struct timeval g_start;

extern "C"

typedef unsigned char   uint8_t;
unsigned int g_detect_number = DETECT_NUM;

#define _SET_STATUS_(status, stat, lbl) do {\
    status = stat; \
    goto lbl; \
}while(0)

#define _CHECK_STATUS_(status, lbl) do {\
    if (NULL == status) {\
        goto lbl; \
    } \
}while(0)

typedef enum {
    NETWORK_UNINIT,
    NETWORK_INIT,
    NETWORK_PREPARING,
    NETWORK_PROCESSING,
} network_status;

typedef struct {
    dev_type type;
    char* value;
    char* info;
}chip_info;

typedef void* (*_aml_module_create)(aml_config* config);
typedef int (*_aml_module_input_set)(void* context,nn_input *pInput);
typedef void* (*_aml_module_output_get)(void* context,aml_output_config_t outconfig);
typedef void (*_aml_module_destroy)(void* context);
typedef void (*_aml_util_getHardwareStatus)(int* customID,int *powerStatus,int *ddk_version);
typedef int (*_aml_read_chip_info)(aml_platform_info_t *platform_info);
typedef void* (*_post_process_all_module)(aml_module_t type,nn_output *pOut);

typedef void *(*_aml_util_mallocBuffer)(void* context, aml_memory_config_t* mem_config, aml_memory_data_t* mem_data);
typedef void (*_aml_util_freeBuffer)(void* context, aml_memory_config_t* mem_config, aml_memory_data_t* mem_data);

typedef struct function_process {
    _aml_module_create              module_create;
    _aml_module_input_set           module_input_set;
    _aml_module_output_get          module_output_get;
    _aml_module_destroy             module_destroy;
    _aml_util_getHardwareStatus     getHardwareStatus;
    _aml_read_chip_info             read_chip_info;
    _post_process_all_module        post_process;
    _aml_util_mallocBuffer          mallocBuffer;
    _aml_util_freeBuffer            freeBuffer;

}network_process;

typedef struct detect_network {
    det_model_type      mtype;
    aml_memory_config_t mem_config_in[ADDRESS_MAX_NUM];
    aml_memory_data_t   memory_data_in[ADDRESS_MAX_NUM];
    aml_memory_config_t mem_config_out[ADDRESS_MAX_NUM];
    aml_memory_data_t   memory_data_out[ADDRESS_MAX_NUM];
    network_status      status;
    void                *context;
    network_process     process;
    void *              handle_id_user;
    void *              handle_id_demo;
}det_network_t, *p_det_network_t;

dev_type g_dev_type = DEV_A311D2;
static det_network_t network[DET_BUTT];

const char* data_file_path = "/etc/nn_data"; //need check adla model path on platfrom with AE team
const char * file_name[DET_BUTT]= {
    "detect_yolo_v3",
    "aml_face_detection",
};

// change to 128 avoid buffer overflow
static char model_path[128];

static const char *coco_names[] = {"person","bicycle","car","motorbike","aeroplane","bus","train",
                "truck","boat","traffic light","fire hydrant","stop sign","parking meter",
                "bench","bird","cat","dog","horse","sheep","cow","elephant","bear","zebra",
                "giraffe","backpack","umbrella","handbag","tie","suitcase","frisbee","skis",
                "snowboard","sports ball","kite","baseball bat","baseball glove","skateboard",
                "surfboard","tennis racket","bottle","wine glass","cup","fork","knife","spoon",
                "bowl","banana","apple","sandwich","orange","broccoli","carrot","hot dog","pizza",
                "donut","cake","chair","sofa","pottedplant","bed","diningtable","toilet","tvmonitor",
                "laptop","mouse","remote","keyboard","cell phone","microwave","oven","toaster","sink",
                "refrigerator","book","clock","vase","scissors","teddy bear","hair drier","toothbrush"};

int get_input_size(det_model_type mtype);

static det_status_t check_input_param(input_image_t imageData, det_model_type modelType)
{
    int ret = DET_STATUS_OK;
    int size=0,imagesize=0;

    if (NULL == imageData.data) {
        LOGE("Data buffer is NULL");
        _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
    }

    if (imageData.pixel_format != PIX_FMT_NV21 && imageData.pixel_format != PIX_FMT_RGB888) {
        LOGE("Current only support RGB888 and NV21");
        _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
    }

    size = get_input_size(modelType);
    if (size == 0) {
        LOGE("Get_model_size fail!");
        _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
    }

    if (imageData.pixel_format == PIX_FMT_RGB888) {
        imagesize = imageData.width*imageData.height*imageData.channel;
        if (size != imagesize) {
            LOGE("Inputsize not match! model size:%d vs input size:%d\n", size, imagesize);
            _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
        }
    }

exit:
    return ret;
}

//ysq
static aml_hw_type_t hw_type = AML_HARDWARE_ADLA;
static void check_and_set_dev_type(det_model_type modelType)
{
    unsigned char plat_type;

    aml_platform_info_t platform_info;
    p_det_network_t net = &network[modelType];

    net->process.read_chip_info(&platform_info);
    plat_type = platform_info.platform_type;
    hw_type = platform_info.hw_type;

    if (hw_type == AML_HARDWARE_VSI_UNIFY)
    {
        int customID,powerStatus,ddk_version;

        net->process.getHardwareStatus(&customID,&powerStatus,&ddk_version);

        switch (customID)
        {
            case 125:
                LOGI("set_dev_type DEV_REVA and setenv 0");
                g_dev_type = DEV_REVA;
                setenv("DEV_TYPE", "0", 0);
                break;
            case 136:
                LOGI("set_dev_type DEV_REVB and setenv 1");
                g_dev_type = DEV_REVB;
                setenv("DEV_TYPE", "1", 0);
                break;
            case 153:
                LOGI("set_dev_type DEV_SM1 and setenv 2");
                g_dev_type = DEV_SM1;
                setenv("DEV_TYPE", "2", 0);
                break;
            default:
                LOGE("set_dev_type fail, please check the type ");
                break;
        }
        LOGD("customID:%d\n", customID);
    }
    else if (hw_type == AML_HARDWARE_ADLA)
    {
        switch (plat_type)
        {
            case 0:
                LOGI("set_dev_type DEV_C308 and setenv 0");
                g_dev_type = DEV_C308;
                setenv("DEV_TYPE", "0", 0);
                break;
            case 1:
                LOGI("set_dev_type DEV_AX201 and setenv 1");
                g_dev_type = DEV_AX201;
                setenv("DEV_TYPE", "1", 0);
                break;
            case 2:
                LOGI("set_dev_type DEV_A311D2 and setenv 2");
                g_dev_type = DEV_A311D2;
                setenv("DEV_TYPE", "2", 0);
                break;
            default:
                LOGE("set_dev_type fail, please check the type:%d\n", plat_type);
                break;
        }
        LOGD("Platform_type:%d\n", plat_type);
    }
    else
    {
        LOGE("check_and_set_dev_type fail, please check the hw_type:%d\n", hw_type);
    }

    LOGD("hw_type:%d\n", hw_type);
}


extern "C"
void *post_process_all_module(aml_module_t type,nn_output *pOut);


det_status_t check_and_set_function(det_model_type modelType)
{
    LOGP("Enter, dlopen so: libnnsdk.so");

    int ret = DET_STATUS_ERROR;
    p_det_network_t net = &network[modelType];

    net->handle_id_user =  dlopen("libnnsdk.so", RTLD_NOW);
    if (NULL == net->handle_id_user) {
        LOGE("dlopen libnnsdk.so failed!,%s",dlerror());
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }

    net->process.module_create = (_aml_module_create)dlsym(net->handle_id_user, "aml_module_create");
    _CHECK_STATUS_(net->process.module_create, exit);

    net->process.module_input_set = (_aml_module_input_set)dlsym(net->handle_id_user, "aml_module_input_set");
    _CHECK_STATUS_(net->process.module_input_set, exit);

    net->process.module_output_get = (_aml_module_output_get)dlsym(net->handle_id_user, "aml_module_output_get");
    _CHECK_STATUS_(net->process.module_output_get, exit);

    net->process.module_destroy = (_aml_module_destroy)dlsym(net->handle_id_user, "aml_module_destroy");
    _CHECK_STATUS_(net->process.module_destroy, exit);

    net->process.getHardwareStatus = (_aml_util_getHardwareStatus)dlsym(net->handle_id_user, "aml_util_getHardwareStatus");
    _CHECK_STATUS_(net->process.getHardwareStatus, exit);

    net->process.read_chip_info = (_aml_read_chip_info)dlsym(net->handle_id_user, "aml_read_chip_info");
    _CHECK_STATUS_(net->process.read_chip_info, exit);

    net->process.mallocBuffer = (_aml_util_mallocBuffer)dlsym(net->handle_id_user, "aml_util_mallocBuffer");
    _CHECK_STATUS_(net->process.mallocBuffer, exit);

    net->process.freeBuffer = (_aml_util_freeBuffer)dlsym(net->handle_id_user, "aml_util_freeBuffer");
    _CHECK_STATUS_(net->process.freeBuffer, exit);

    net->process.post_process = (_post_process_all_module)post_process_all_module;

    ret = DET_STATUS_OK;
exit:
    LOGP("Leave, dlopen so: libnnsdk.so, ret=%d", ret);
     return ret;
}

int get_input_size(det_model_type mtype)
{
    int input_size = 0;
    switch (mtype)
    {
    case DET_YOLO_V3:
        input_size = 416*416*3;
        break;
    case DET_AML_FACE_DETECTION:
        input_size = 640*384*3;
        break;
    default:
        break;
    }
    return input_size;
}

det_status_t det_get_model_name(const char * data_file_path, dev_type type,det_model_type mtype)
{
    int ret = DET_STATUS_OK;
    int index = 0;

    switch (mtype)
    {
        case DET_YOLO_V3:
            index = 0;
            break;
        case DET_AML_FACE_DETECTION:
            index = 1;
            break;
        default:
            break;
    }

    if (hw_type == AML_HARDWARE_VSI_UNIFY)
    {
        switch (type)
        {
            case DEV_REVA:
                sprintf(model_path, "%s/%s/%s_7d.nb", data_file_path,file_name[index],file_name[index]);
                break;
            case DEV_REVB:
                sprintf(model_path, "%s/%s/%s_88.nb", data_file_path,file_name[index],file_name[index]); // /etc/nn_data/detect_yolo_v3/detect_yolo_v3_88.nb
                break;
            case DEV_SM1:
                sprintf(model_path, "%s/%s/%s_99.nb", data_file_path,file_name[index],file_name[index]);
                break;
            default:
                break;
        }
    }
    else if (hw_type == AML_HARDWARE_ADLA)
    {
        switch (type) {
            case DEV_C308:
                sprintf(model_path, "%s/%s/%s_A0001.adla", data_file_path,file_name[index],file_name[index]);
                break;
            case DEV_AX201:
                sprintf(model_path, "%s/%s/%s_A0002.adla", data_file_path,file_name[index],file_name[index]);
                break;
            case DEV_A311D2:
                sprintf(model_path, "%s/%s/%s_A0003.adla", data_file_path, file_name[index],file_name[index]); // /etc/nn_data/detect_yolo_v3/detect_yolo_v3_A0003.adla
                break;
            default:
                break;
        }
    }

    return ret;
}

det_status_t det_set_model(det_model_type modelType)
{
    aml_config config;
    int ret = DET_STATUS_OK;
    int input_size[ADDRESS_MAX_NUM] = {0};
    int output_size[ADDRESS_MAX_NUM] = {0};
    p_det_network_t net = &network[modelType];

    LOGP("Enter, modeltype:%d", modelType);
    if (modelType >= DET_BUTT) {
        LOGE("Det_set_model fail, modelType >= BUTT");
        _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
    }

    ret = check_and_set_function(modelType);
    check_and_set_dev_type(modelType);
    if (ret) {
        LOGE("ModelType so open failed or Not support now!!");
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }
    memset(&config,0,sizeof(aml_config));
    LOGI("Start create Model, data_file_path=%s",data_file_path);
    det_get_model_name(data_file_path,g_dev_type,modelType);

    if (hw_type == AML_HARDWARE_VSI_UNIFY)
    {
        config.nbgType = NN_NBG_FILE;
        switch (modelType)
        {
            case DET_YOLO_V3:
                input_size[0] = 416*416*3;
                output_size[0] = 13*13*255;
                output_size[1] = 26*26*255;
                output_size[2] = 52*52*255;

                net->mem_config_in[0].cache_type = AML_WITH_CACHE;
                net->mem_config_in[0].memory_type = AML_VIRTUAL_ADDR;
                net->mem_config_in[0].direction = AML_MEM_DIRECTION_READ_WRITE;
                net->mem_config_in[0].index = 0;
                net->mem_config_in[0].mem_size = input_size[0];
                net->process.mallocBuffer(net->context, &net->mem_config_in[0], &net->memory_data_in[0]);
                config.inOut.inAddr[0] = (uint8_t *)net->memory_data_in[0].viraddr;

                for (int i = 0; i < 3; i++)
                {
                    net->mem_config_out[i].cache_type = AML_WITH_CACHE;
                    net->mem_config_out[i].memory_type = AML_VIRTUAL_ADDR;
                    net->mem_config_out[i].direction = AML_MEM_DIRECTION_READ_WRITE;
                    net->mem_config_out[i].index = i;
                    net->mem_config_out[i].mem_size = output_size[i];
                    net->process.mallocBuffer(net->context, &net->mem_config_out[i], &net->memory_data_out[i]);
                    config.inOut.outAddr[i] = (uint8_t *)net->memory_data_out[i].viraddr;
                }

                config.modelType = DARKNET;
                break;
            case DET_AML_FACE_DETECTION:
                config.modelType = TENSORFLOW;
                break;
            default:
                break;
        }
    }
    else if (hw_type == AML_HARDWARE_ADLA)
    {
        config.nbgType = NN_ADLA_FILE;
        config.modelType = ADLA_LOADABLE;
    }

    LOGI("module_create, model_path=%s",model_path);
    config.path = (const char *)model_path;
    config.typeSize = sizeof(aml_config);

    net->context = net->process.module_create(&config);
    if (net->context == NULL) {
    LOGE("Model_create fail, file_path=%s, dev_type=%d", data_file_path, g_dev_type);
    _SET_STATUS_(ret, DET_STATUS_CREATE_NETWORK_FAIL, exit);
    }

    if (hw_type == AML_HARDWARE_ADLA)//vsi要在模型加载前分配buf，adla要在加载之后分配
    {
        switch (modelType)
        {
            case DET_YOLO_V3:
                input_size[0] = 416*416*3;

                net->mem_config_in[0].cache_type = AML_WITH_CACHE;
                net->mem_config_in[0].memory_type = AML_VIRTUAL_ADDR;
                net->mem_config_in[0].direction = AML_MEM_DIRECTION_READ_WRITE;
                net->mem_config_in[0].index = 0;
                net->mem_config_in[0].mem_size = input_size[0];
                net->process.mallocBuffer(net->context, &net->mem_config_in[0], &net->memory_data_in[0]);
                break;
            case DET_AML_FACE_DETECTION:
                input_size[0] = 640*384*3;

                net->mem_config_in[0].cache_type = AML_WITH_CACHE;
                net->mem_config_in[0].memory_type = AML_VIRTUAL_ADDR;
                net->mem_config_in[0].direction = AML_MEM_DIRECTION_READ_WRITE;
                net->mem_config_in[0].index = 0;
                net->mem_config_in[0].mem_size = input_size[0];
                net->process.mallocBuffer(net->context, &net->mem_config_in[0], &net->memory_data_in[0]);
                break;
            default:
                break;
        }
    }

    net->mtype = modelType;
    // LOGI("input_ptr size=%d, addr=%x", input_size[0], net->memory_data_in[0].memory);
    net->status = NETWORK_INIT;

exit:
    LOGP("Leave, modeltype:%d", modelType);
    return ret;
}

det_status_t det_get_model_size(det_model_type modelType, int *width, int *height, int *channel)
{
    LOGP("Enter, modeltype:%d", modelType);

    int ret = DET_STATUS_OK;
    p_det_network_t net = &network[modelType];
    if (!net->status) {
        LOGE("Model has not created! modeltype:%d", modelType);
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }
    switch (modelType)
    {
    case DET_YOLO_V3:
        *width = 416;
        *height = 416;
        *channel = 3;
        break;
    case DET_AML_FACE_DETECTION:
        *width = 640;
        *height = 384;
        *channel = 3;
        break;
    default:
        break;
    }
exit:
    LOGP("Leave, modeltype:%d", modelType);
    return ret;
}

det_status_t det_set_input(input_image_t imageData, det_model_type modelType)
{
    nn_input inData;
    int ret = DET_STATUS_OK;
    p_det_network_t net = &network[modelType];
    int i,j,tmpdata,nn_width, nn_height, channels,offset;

    LOGP("Enter, modeltype:%d", modelType);
    if (!net->status) {
        LOGE("Model has not created! modeltype:%d", modelType);
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }

    gettimeofday(&g_start, NULL);

    ret = check_input_param(imageData, modelType);
    if (ret) {
        LOGE("Check_input_param fail.");
        _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
    }

    // need check u8 or i8, prepare diff preprocess
    if (hw_type == AML_HARDWARE_VSI_UNIFY)
    {
        switch (modelType)
        {
            case DET_YOLO_V3:
                det_get_model_size(modelType,&nn_width, &nn_height, &channels);
                for (i = 0; i < channels; i++)
                {
                    offset = nn_width * nn_height * i;
                    for (j = 0; j < nn_width * nn_height; j++)
                    {
                        tmpdata = (imageData.data[j * channels + i]);
                        ((unsigned char *)net->memory_data_in[0].viraddr)[j + offset] = tmpdata;
                    }
                }
                break;
            case DET_AML_FACE_DETECTION:
                break;
            default:
                break;
        }
    }
    else if (hw_type == AML_HARDWARE_ADLA)
    {
        switch (modelType)
        {
            case DET_YOLO_V3:
            case DET_AML_FACE_DETECTION:
                memcpy(net->memory_data_in[0].viraddr,imageData.data,net->mem_config_in[0].mem_size);
                break;
            default:
                break;
        }
    }

    inData.input_type = INPUT_DMA_DATA;
    inData.input_index = 0;
    inData.size = net->mem_config_in[0].mem_size;
    inData.input = (uint8_t*)net->memory_data_in[0].memory;
    ret = net->process.module_input_set(net->context, &inData);

    if (ret)
    {
        LOGE("Set input fail.");
        _SET_STATUS_(ret, DET_STATUS_SET_INPUT_ERROR, exit);
    }
    net->status = NETWORK_PREPARING;

exit:
    LOGP("Leave, modeltype:%d", modelType);

    struct timeval end;
    double time_total;
    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - g_start.tv_sec)*1000000.0 + (end.tv_usec - g_start.tv_usec);
    g_start = end;
    LOGI("det_set_input, time=%lf uS \n", time_total);

    return ret;
}

det_status_t det_set_param(det_model_type modelType, det_param_t param)
{
    LOGP("Enter, modeltype:%d", modelType);
    int ret = DET_STATUS_OK;
    int number = param.param.det_param.detect_num;
    LOGI("detect num is %d\n",number);

    p_det_network_t net = &network[modelType];
    if (!net->status) {
        LOGE("Model has not created! modeltype:%d", modelType);
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }
    switch (modelType)
    {
    case DET_YOLO_V3:
    case DET_AML_FACE_DETECTION:
        if (number < DETECT_NUM || number > MAX_DETECT_NUM)
        {
            ret = -1;
        }
        else
        {
            g_detect_number = number;
        }
        LOGI("g_detect_number is %d\n",g_detect_number);
        break;
    default:
        break;
    }

exit:
    LOGP("Leave, modeltype:%d", modelType);
    return ret;
}

det_status_t det_get_param(det_model_type modelType,det_param_t *param)
{
    LOGP("Enter, modeltype:%d", modelType);
    int ret = DET_STATUS_OK;

    p_det_network_t net = &network[modelType];
    if (!net->status) {
        LOGE("Model has not created! modeltype:%d", modelType);
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }

    switch (modelType)
    {
    case DET_YOLO_V3:
    case DET_AML_FACE_DETECTION:
        param->param.det_param.detect_num = g_detect_number;
        LOGI("g_detect_number is %d\n",g_detect_number);
        break;
    default:
        break;
    }

exit:
    LOGP("Leave, modeltype:%d", modelType);
    return ret;
}

aml_module_t get_sdk_modeltype(det_model_type modelType)
{
    aml_module_t type = YOLO_V3;
    switch (modelType)
    {
    case DET_YOLO_V3:
        type = YOLO_V3;
        break;
    case DET_AML_FACE_DETECTION:
        type = FACE_LANDMARK_5;
        break;
    default:
        break;
    }
    return type;
}

void post_process(det_model_type modelType,void* out,pDetResult resultData)
{
    LOGP("Enter, post_process modelType:%d", modelType);

    face_landmark5_out_t           *face_detect_out            = NULL;
    yolov3_out_t                   *yolov3_out                 = NULL;

    float left = 0, right = 0, top = 0, bot=0, prob = 0;
    unsigned int i = 0, input_width = 0, input_high = 0;
    int label_num = 0;

    switch (modelType)
    {
        case DET_YOLO_V3:
            input_width = 416, input_high = 416;
            yolov3_out = (yolov3_out_t *)out;
            // printf("yolov3_out->detNum = %d\n", yolov3_out->detNum);
            resultData->result.det_result.detect_num = yolov3_out->detNum;
            resultData->result.det_result.point = (det_position_float_t *)malloc(sizeof(det_position_float_t) * resultData->result.det_result.detect_num);
            resultData->result.det_result.result_name = (det_classify_result_t *)malloc(sizeof(det_classify_result_t) * resultData->result.det_result.detect_num);
            // printf("detNum = %d\n", yolov3_out->detNum);
            for (i=0;i<yolov3_out->detNum;i++)
            {
                if (i >= g_detect_number) break;
                left  = (yolov3_out->pBox[i].x-yolov3_out->pBox[i].w/2.)*input_width;
                right = (yolov3_out->pBox[i].x+yolov3_out->pBox[i].w/2.)*input_width;
                top   = (yolov3_out->pBox[i].y-yolov3_out->pBox[i].h/2.)*input_high;
                bot   = (yolov3_out->pBox[i].y+yolov3_out->pBox[i].h/2.)*input_high;
                prob = yolov3_out->pBox[i].score;
                label_num = int(yolov3_out->pBox[i].objectClass);

                if (left < 2) left = 2;
                if (right > input_width-2) right = input_width-2;
                if (top < 2) top = 2;
                if (bot > input_high-2) bot = input_high-2;

                resultData->result.det_result.point[i].type = DET_RECTANGLE_TYPE;
                resultData->result.det_result.point[i].point.rectPoint.left = left;
                resultData->result.det_result.point[i].point.rectPoint.right = right;
                resultData->result.det_result.point[i].point.rectPoint.top = top;
                resultData->result.det_result.point[i].point.rectPoint.bottom = bot;
                resultData->result.det_result.point[i].point.rectPoint.score = prob;
                resultData->result.det_result.result_name[i].label_id = label_num;
                memcpy(resultData->result.det_result.result_name[i].label_name, coco_names[label_num], sizeof(coco_names[label_num]));
                LOGI("num:%d, class:%s, label_num:%d, left:%f, right:%f, top:%f, bot:%f, score:%f\n", i+1,
                                                                                resultData->result.det_result.result_name[i].label_name,
                                                                                resultData->result.det_result.result_name[i].label_id,
                                                                                resultData->result.det_result.point[i].point.rectPoint.left,
                                                                                resultData->result.det_result.point[i].point.rectPoint.right,
                                                                                resultData->result.det_result.point[i].point.rectPoint.top,
                                                                                resultData->result.det_result.point[i].point.rectPoint.bottom,
                                                                                resultData->result.det_result.point[i].point.rectPoint.score);
            }
            break;
        case DET_AML_FACE_DETECTION:
            input_width = 640, input_high = 384;
            face_detect_out = (face_landmark5_out_t *)out;
            resultData->result.det_result.detect_num = face_detect_out->detNum;
            resultData->result.det_result.point = (det_position_float_t *)malloc(sizeof(det_position_float_t) * resultData->result.det_result.detect_num);
            for (i=0;i<face_detect_out->detNum;i++)
            {
                if (i >= g_detect_number) break;
                left = (face_detect_out->facebox[i].x)*input_width;
                right = (face_detect_out->facebox[i].x + face_detect_out->facebox[i].w)*input_width;
                top = (face_detect_out->facebox[i].y)*input_high;
                bot = (face_detect_out->facebox[i].y + face_detect_out->facebox[i].h)*input_high;
                prob = face_detect_out->facebox[i].score;

            if (left < 2) left = 2;
            if (right > input_width-2) right = input_width-2;
            if (top < 2) top = 2;
            if (bot > input_high-2) bot = input_high-2;

            resultData->result.det_result.point[i].type = DET_RECTANGLE_TYPE;
            resultData->result.det_result.point[i].point.rectPoint.left = left;
            resultData->result.det_result.point[i].point.rectPoint.right = right;
            resultData->result.det_result.point[i].point.rectPoint.top = top;
            resultData->result.det_result.point[i].point.rectPoint.bottom = bot;
            resultData->result.det_result.point[i].point.rectPoint.score = prob;

            resultData->result.det_result.point[i].tpts.floatX[0] = face_detect_out->pos[i][0].x*input_width;
            resultData->result.det_result.point[i].tpts.floatY[0] = face_detect_out->pos[i][0].y*input_high;

            resultData->result.det_result.point[i].tpts.floatX[1] = face_detect_out->pos[i][1].x*input_width;
            resultData->result.det_result.point[i].tpts.floatY[1] = face_detect_out->pos[i][1].y*input_high;

            resultData->result.det_result.point[i].tpts.floatX[2] = face_detect_out->pos[i][2].x*input_width;
            resultData->result.det_result.point[i].tpts.floatY[2] = face_detect_out->pos[i][2].y*input_high;

            resultData->result.det_result.point[i].tpts.floatX[3] = face_detect_out->pos[i][3].x*input_width;
            resultData->result.det_result.point[i].tpts.floatY[3] = face_detect_out->pos[i][3].y*input_high;

            resultData->result.det_result.point[i].tpts.floatX[4] = face_detect_out->pos[i][4].x*input_width;
            resultData->result.det_result.point[i].tpts.floatY[4] = face_detect_out->pos[i][4].y*input_high;

            // printf("face_number:%d, left=%f, right=%f, top=%f, bot=%f, score:%f\n", i+1,
            //                                                                         resultData->result.det_result.point[i].point.rectPoint.left,
            //                                                                         resultData->result.det_result.point[i].point.rectPoint.right,
            //                                                                         resultData->result.det_result.point[i].point.rectPoint.top,
            //                                                                         resultData->result.det_result.point[i].point.rectPoint.bottom,
            //                                                                         resultData->result.det_result.point[i].point.rectPoint.score);
            // printf("left_eye.x = %f ,   left_eye.y = %f \n",                        resultData->result.det_result.point[i].tpts.floatX[0], resultData->result.det_result.point[i].tpts.floatY[0]);
            // printf("right_eye.x = %f,   right_eye.y = %f \n",                       resultData->result.det_result.point[i].tpts.floatX[1], resultData->result.det_result.point[i].tpts.floatY[1]);
            // printf("nose.x = %f,        nose.y = %f \n",                            resultData->result.det_result.point[i].tpts.floatX[2], resultData->result.det_result.point[i].tpts.floatY[2]);
            // printf("left_mouth.x = %f,  left_mouth.y = %f \n",                      resultData->result.det_result.point[i].tpts.floatX[3], resultData->result.det_result.point[i].tpts.floatY[3]);
            // printf("right_mouth.x = %f, right_mouth.y = %f\n\n",                    resultData->result.det_result.point[i].tpts.floatX[4], resultData->result.det_result.point[i].tpts.floatY[4]);
        }
        break;
    default:
        break;
    }

    return;
}


det_status_t det_get_result(pDetResult resultData, det_model_type modelType)
{
    aml_output_config_t outconfig;
    aml_module_t modelType_sdk;
    int ret = DET_STATUS_OK;

    struct timeval end;
    double time_total;
    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - g_start.tv_sec)*1000000.0 + (end.tv_usec - g_start.tv_usec);
    g_start = end;
    LOGI("det_set_input-det_get_result, time=%lf uS \n", time_total);

    p_det_network_t net = &network[modelType];
    void *out;
    nn_output *nn_out;
    LOGP("Enter, modeltype:%d", modelType);
    if (NETWORK_PREPARING != net->status) {
        LOGE("Model not create or not prepared! status=%d", net->status);
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }
    outconfig.typeSize = sizeof(aml_output_config_t);
    modelType_sdk = get_sdk_modeltype(modelType);

    outconfig.format = AML_OUTDATA_DMA;

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - g_start.tv_sec)*1000000.0 + (end.tv_usec - g_start.tv_usec);
    g_start = end;
    LOGI("before AML_PERF_OUTPUT_SET, time=%lf uS \n", time_total);

    outconfig.perfMode = AML_PERF_OUTPUT_SET;
    nn_out = (nn_output*)net->process.module_output_get(net->context,outconfig);

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - g_start.tv_sec)*1000000.0 + (end.tv_usec - g_start.tv_usec);
    g_start = end;
    LOGI("AML_PERF_OUTPUT_SET, time=%lf uS \n", time_total);

    outconfig.perfMode = AML_PERF_INFERENCE;
    nn_out = (nn_output*)net->process.module_output_get(net->context,outconfig);

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - g_start.tv_sec)*1000000.0 + (end.tv_usec - g_start.tv_usec);
    g_start = end;
    LOGI("AML_PERF_INFERENCE, time=%lf uS \n", time_total);

    outconfig.perfMode = AML_PERF_OUTPUT_GET;
    nn_out = (nn_output*)net->process.module_output_get(net->context,outconfig);

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - g_start.tv_sec)*1000000.0 + (end.tv_usec - g_start.tv_usec);
    g_start = end;
    LOGI("AML_PERF_OUTPUT_GET, time=%lf uS \n", time_total);

    out = (void*)net->process.post_process(modelType_sdk, nn_out);

    if (out == NULL) {
        LOGE("Process Net work fail");
        _SET_STATUS_(ret, DET_STATUS_PROCESS_NETWORK_FAIL, exit);
    }

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - g_start.tv_sec)*1000000.0 + (end.tv_usec - g_start.tv_usec);
    g_start = end;
    LOGI("net->process.post_process, time=%lf uS \n", time_total);

    post_process(modelType,out,resultData);
    net->status = NETWORK_PROCESSING;

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - g_start.tv_sec)*1000000.0 + (end.tv_usec - g_start.tv_usec);
    g_start = end;
    LOGI("post_process, time=%lf uS \n", time_total);
exit:
    LOGP("Leave, modeltype:%d", modelType);

    return ret;
}


det_status_t det_release_model(det_model_type modelType)
{
    int ret = DET_STATUS_OK;
    p_det_network_t net = &network[modelType];

    LOGP("Enter, modeltype:%d", modelType);
    if (!net->status) {
        LOGW("Model has benn released!");
        _SET_STATUS_(ret, DET_STATUS_OK, exit);
    }

    if (hw_type == AML_HARDWARE_VSI_UNIFY)//vsi这边需要对输入输出的DMA buf都进行释放
    {
        switch (modelType)
        {
            case DET_YOLO_V3:
                if (net->memory_data_in[0].viraddr)
                    net->process.freeBuffer(net->context, &net->mem_config_in[0], &net->memory_data_in[0]);
                if (net->memory_data_out[0].viraddr)
                    net->process.freeBuffer(net->context, &net->mem_config_out[0], &net->memory_data_out[0]);
                if (net->memory_data_out[1].viraddr)
                    net->process.freeBuffer(net->context, &net->mem_config_out[1], &net->memory_data_out[1]);
                if (net->memory_data_out[2].viraddr)
                    net->process.freeBuffer(net->context, &net->mem_config_out[2], &net->memory_data_out[2]);
                break;
            case DET_AML_FACE_DETECTION:
                break;
            default:
                break;
        }
    }
    else if (hw_type == AML_HARDWARE_ADLA)//adla只需要释放输入DMA buf
    {
        switch (modelType)
        {
            case DET_YOLO_V3:
            case DET_AML_FACE_DETECTION:
                if (net->memory_data_in[0].viraddr)
                    net->process.freeBuffer(net->context, &net->mem_config_in[0], &net->memory_data_in[0]);
                break;
            default:
                break;
        }
    }

    if (net->context != NULL) {
        net->process.module_destroy(net->context);
        net->context = NULL;
    }

    dlclose(net->handle_id_user);
    net->handle_id_user = NULL;
    // dlclose(net->handle_id_demo);
    // net->handle_id_demo = NULL;
    net->status = NETWORK_UNINIT;

exit:
    LOGP("Leave, modeltype:%d", modelType);
    return ret;
}

det_status_t det_set_log_config(det_debug_level_t level,det_log_format_t output_format)
{
    LOGP("Enter, level:%d", level);
    det_set_log_level(level, output_format);
    LOGP("Leave, level:%d", level);
    return 0;
}


/////////////////////////////////////////////////////////////////////////////////////////////
// for async mode, better performance
/////////////////////////////////////////////////////////////////////////////////////////////

// det_status_t det_set_data_to_NPU(input_image_t imageData, det_model_type modelType){
//     int ret = DET_STATUS_OK;
//     p_det_network_t net = &network[modelType];
//     struct timeval start;
//     struct timeval end;
//     double time_total;
//     gettimeofday(&start, NULL);

//     int i,j,tmpdata,nn_width, nn_height, channels,offset;

//     ret = check_input_param(imageData, modelType);
//     if (ret) {
//         LOGE("Check_input_param fail.");
//         _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
//     }


//     if (hw_type == AML_HARDWARE_VSI_UNIFY)
//         {
//             switch (modelType)
//             {
//                 case DET_YOLO_V3:
//                     det_get_model_size(modelType,&nn_width, &nn_height, &channels);
//                     for (i = 0; i < channels; i++)
//                     {
//                         offset = nn_width * nn_height * i;
//                         for (j = 0; j < nn_width * nn_height; j++)
//                         {
//                             tmpdata = (imageData.data[j * channels + i]);
//                             ((unsigned char *)net->memory_data_in[0].viraddr)[j + offset] = tmpdata;
//                         }
//                     }

//                     gettimeofday(&end, NULL);
//                     time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
//                     start = end;
//                     LOGI("det_set_input_to_NPU_u8_change, time=%lf uS \n", time_total);
//                     break;
//                 case DET_AML_FACE_DETECTION:
//                     break;
//                 default:
//                     break;
//             }
//         }
//         else if (hw_type == AML_HARDWARE_ADLA)
//         {
//             switch (modelType)
//             {
//                 case DET_YOLO_V3:
//                 case DET_AML_FACE_DETECTION:
//                     memcpy(net->memory_data_in[0].viraddr,imageData.data,net->mem_config_in[0].mem_size);
//                     break;
//                 default:
//                     break;
//             }
//         }
//     return ret;

// exit:
//     LOGI("Leave, modeltype:%d", modelType);

//     return ret;

// }


static det_status_t det_set_input_to_NPU(input_image_t imageData, det_model_type modelType)
{
    nn_input inData;
    int ret = DET_STATUS_OK;
    p_det_network_t net = &network[modelType];
    static int currentIndex = 0;



    struct timeval start;
    struct timeval end;
    double time_total;
    gettimeofday(&start, NULL);

    LOGI("Enter, modeltype:%d", modelType);

    // need check u8 or i8, prepare diff preprocess
    //zyadd
    int i,j,tmpdata,nn_width, nn_height, channels,offset;
    ret = check_input_param(imageData, modelType);
    if (ret) {
        LOGE("Check_input_param fail.");
        _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
    }


    if (hw_type == AML_HARDWARE_VSI_UNIFY)
        {
            switch (modelType)
            {
                case DET_YOLO_V3:
                    det_get_model_size(modelType,&nn_width, &nn_height, &channels);
                    for (i = 0; i < channels; i++)
                    {
                        offset = nn_width * nn_height * i;
                        for (j = 0; j < nn_width * nn_height; j++)
                        {
                            tmpdata = (imageData.data[j * channels + i]);
                            ((unsigned char *)net->memory_data_in[0].viraddr)[j + offset] = tmpdata;
                        }
                    }

                    gettimeofday(&end, NULL);
                    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
                    start = end;
                    LOGI("det_set_input_to_NPU_u8_change, time=%lf uS \n", time_total);
                    break;
                case DET_AML_FACE_DETECTION:
                    break;
                default:
                    break;
            }
        }
        else if (hw_type == AML_HARDWARE_ADLA)
        {
            switch (modelType)
            {
                case DET_YOLO_V3:
                case DET_AML_FACE_DETECTION:
                    memcpy(net->memory_data_in[0].viraddr,imageData.data,net->mem_config_in[0].mem_size);
                    break;
                default:
                    break;
            }
            //yi.zhang1 add ,for debug dump frame
            #if 0
                string filename = "output_image_" + std::to_string(currentIndex) + ".raw";
                printf("we now get the %d frame\n",currentIndex);
                ofstream imageFile(filename, std::ios::binary);
                imageFile.write(reinterpret_cast<const char*>(imageData.data), imageData.width * imageData.height * imageData.channel);
                imageFile.close();
                currentIndex++;
            #endif
        }

    inData.input_type = INPUT_DMA_DATA;
    inData.input_index = 0;
    inData.size = net->mem_config_in[0].mem_size;
    inData.input = (uint8_t*)net->memory_data_in[0].memory;
    ret = net->process.module_input_set(net->context, &inData);

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    LOGI("net->process.module_input_set, time=%lf uS \n", time_total);

    if (ret)
    {
        LOGE("Set input fail.");
        _SET_STATUS_(ret, DET_STATUS_SET_INPUT_ERROR, exit);
    }
    net->status = NETWORK_PREPARING;

exit:
    LOGI("Leave, modeltype:%d", modelType);

    return ret;
}



static det_status_t det_get_outconfig(det_model_type modelType, aml_output_config_t *pOutconfig)
{
    int ret = DET_STATUS_OK;
    pOutconfig->typeSize = sizeof(aml_output_config_t);
    pOutconfig->format = AML_OUTDATA_DMA;

    return ret;
}



static det_status_t det_set_output_to_NPU(det_model_type modelType, aml_output_config_t *pOutconfig)
{
    int ret = DET_STATUS_OK;
    p_det_network_t net = &network[modelType];

    LOGI("Enter, modeltype:%d", modelType);
    if (NETWORK_PREPARING != net->status) {
        LOGE("Model not create or not prepared! status=%d", net->status);
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }

    pOutconfig->perfMode = AML_PERF_OUTPUT_SET;
    net->process.module_output_get(net->context, *pOutconfig);

exit:
    LOGI("Leave, modeltype:%d", modelType);
    return ret;
}

//nn_output *nn_out = NULL;
// trigger NPU HW thread will call this function
det_status_t det_trigger_inference(input_image_t imageData, det_model_type modelType)
{
    aml_output_config_t outconfig;
    p_det_network_t net = &network[modelType];
    int ret = DET_STATUS_OK;

    LOGI("Enter, modeltype:%d", modelType);

    struct timeval start;
    struct timeval end;
    double time_total;
    gettimeofday(&start, NULL);

    /////////////////////////////////////////////////////////////////////////////////
    // 1. set input
    det_set_input_to_NPU(imageData, modelType);

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    LOGI("det_set_input_to_NPU, time=%lf uS \n", time_total);

    /////////////////////////////////////////////////////////////////////////////////
    // get outconfig
    det_get_outconfig(modelType, &outconfig);

    // 2. set output
    det_set_output_to_NPU(modelType, &outconfig);

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    LOGI("det_set_output_to_NPU, time=%lf uS \n", time_total);

    /////////////////////////////////////////////////////////////////////////////////
    // set triggler inference
    outconfig.perfMode = AML_PERF_INFERENCE;
    net->process.module_output_get(net->context, outconfig);

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    LOGI("AML_PERF_INFERENCE, time=%lf uS \n", time_total);

    LOGI("Leave, modeltype:%d", modelType);
    return ret;
}



// analysis output, and notify next pipe , result thread will call this function
det_status_t det_get_inference_result(pDetResult resultData, det_model_type modelType)
{
    aml_output_config_t outconfig;
    p_det_network_t net = &network[modelType];
    int ret = DET_STATUS_OK;
    nn_output *nn_out;

    void *out;
    LOGI("Enter, modeltype:%d", modelType);

    struct timeval start;
    struct timeval end;
    double time_total;
    gettimeofday(&start, NULL);

    // get outconfig
    det_get_outconfig(modelType, &outconfig);


    outconfig.perfMode = AML_PERF_OUTPUT_GET;
    nn_out = (nn_output*)net->process.module_output_get(net->context, outconfig);

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    LOGI("AML_PERF_OUTPUT_GET, num=%d, out[0]=%d, out[1]=%d, out[2]=%d, time=%lf uS\n", nn_out->num, nn_out->out[0].size, nn_out->out[1].size, nn_out->out[2].size, time_total);

    aml_module_t modelType_sdk;
    modelType_sdk = get_sdk_modeltype(modelType);
    out = (void*)net->process.post_process(modelType_sdk, nn_out);

    if (out == NULL) {
        LOGE("Process Net work fail");
        _SET_STATUS_(ret, DET_STATUS_PROCESS_NETWORK_FAIL, exit);
    }

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    LOGI("nnsdk, time=%lf uS \n", time_total);

    post_process(modelType, out, resultData);
    net->status = NETWORK_PROCESSING;

    gettimeofday(&end, NULL);
    time_total = (end.tv_sec - start.tv_sec)*1000000.0 + (end.tv_usec - start.tv_usec);
    start = end;
    LOGI("post_process, time=%lf uS \n", time_total);
exit:
    LOGI("Leave, modeltype:%d", modelType);
    return ret;
}




