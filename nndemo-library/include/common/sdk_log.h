/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * aml_nnsdk
 */

#ifndef _SDK_LOG_H
#define _SDK_LOG_H

#ifdef __cplusplus
extern "C"{
#endif

// #define ANDROID_SDK

#define AML_NNSDK_STATUS_OK  0

#ifdef ANDROID_SDK
#include<android/log.h>
#include "cutils/properties.h"
#endif

#include<stdlib.h>
#define PROPERTY_KEY_MAX   32
#define PROPERTY_VALUE_MAX  92
#define DEBUG_BUFFER_LEN 512

int property_get(const char *key, char*value, const char *default_value);
int property_set(const char *key, const char *value);
int property_list(void (*propfn)(const char *key, const char *value, void*cookie), void *cookie);

static char prop[PROPERTY_VALUE_MAX] = "0";

typedef enum{
    SDK_LOG_NULL = -1,          ///< close all log output
    SDK_LOG_TERMINAL,               ///< set log print to terminal
    SDK_LOG_FILE,                   ///< set log print to file
    SDK_LOG_SYSTEM                ///< set log print to system
}sdk_log_format_t;

typedef enum NNSDK_Status
{
    NNsdkStatus_Success              = 0,
    NNsdkStatus_Failed               = 1, // general error, unknown
    NNsdkStatus_OutOfMemory          = 2,
    NNsdkStatus_OutOfDeviceMemory    = 3,
    NNsdkStatus_BadParameter         = 4,
    NNsdkStatus_DeviceLost           = 5,
} NNSDK_Status;

typedef enum{
    SDK_DEBUG_LEVEL_RELEASE = -1,   ///< close debug
    SDK_DEBUG_LEVEL_ERROR,          ///< error level,hightest level system will exit and crash
    SDK_DEBUG_LEVEL_WARN,           ///< warning, system continue working,but something maybe wrong
    SDK_DEBUG_LEVEL_INFO,           ///< info some value if needed
    SDK_DEBUG_LEVEL_PROCESS,        ///< default,some process print
    SDK_DEBUG_LEVEL_DEBUG           ///< debug level,just for debug
}sdk_debug_level_t;


typedef enum{
    CONVERT_DEBUG_LEVEL_RELEASE = -1,   ///< close debug
    CONVERT_DEBUG_LEVEL_ERROR,          ///< error level,hightest level system will exit and crash
    CONVERT_DEBUG_LEVEL_WARN,           ///< warning, system continue working,but something maybe wrong
    CONVERT_DEBUG_LEVEL_INFO,           ///< info some value if needed
    CONVERT_DEBUG_LEVEL_PROCESS,        ///< default,some process print
    CONVERT_DEBUG_LEVEL_DEBUG           ///< debug level,just for debug
}convert_debug_level_t;

#define LOG_TAG "NN_SDK:"
#define CONVERT_LOG_TAG "CONVERT:"
extern sdk_debug_level_t sdk_debug_level;
extern convert_debug_level_t convert_debug_level;
#ifdef ANDROID_SDK
#define convert_loge(convert_log_level, fmt, args...) \
    __android_log_print(ANDROID_LOG_ERROR, CONVERT_LOG_TAG, "[%s:%d]" fmt, __FUNCTION__, __LINE__, ##args);

#define convert_logw(convert_log_level, fmt, args...) \
    if (convert_log_level <= convert_debug_level) \
    {__android_log_print(ANDROID_LOG_WARN, CONVERT_LOG_TAG, "[%s:%d]" fmt, __FUNCTION__, __LINE__, ##args);}

#define convert_logi(convert_log_level, fmt, args...) \
    if (convert_log_level <= convert_debug_level) \
    {__android_log_print(ANDROID_LOG_INFO, CONVERT_LOG_TAG, "[%s:%d]" fmt, __FUNCTION__, __LINE__, ##args);}

#define convert_logd(convert_log_level, fmt, args...) \
    if (convert_log_level <= convert_debug_level) \
    {__android_log_print(ANDROID_LOG_DEBUG, CONVERT_LOG_TAG, "[%s:%d]" fmt, __FUNCTION__, __LINE__, ##args);}

#define convert_logp(convert_log_level, fmt, args...) \
    if (convert_log_level <= convert_debug_level) \
    {__android_log_print(ANDROID_LOG_DEBUG, CONVERT_LOG_TAG, "[%s:%d]" fmt, __FUNCTION__, __LINE__, ##args);}


#define LOGE(nnsdk_log_level, fmt, args...) \
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "[%s:%d]" fmt, __FUNCTION__, __LINE__, ##args);

#define LOGW(nnsdk_log_level, fmt, args...) \
    if (nnsdk_log_level <= sdk_debug_level) \
    {__android_log_print(ANDROID_LOG_WARN, LOG_TAG, "[%s:%d]" fmt, __FUNCTION__, __LINE__, ##args);}

#define LOGI(nnsdk_log_level, fmt, args...) \
    if (nnsdk_log_level <= sdk_debug_level) \
    {__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "[%s:%d]" fmt, __FUNCTION__, __LINE__, ##args);}

#define LOGD(nnsdk_log_level, fmt, args...) \
    if (nnsdk_log_level <= sdk_debug_level) \
    {__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "[%s:%d]" fmt, __FUNCTION__, __LINE__, ##args);}

#define LOGP(nnsdk_log_level, fmt, args...) \
    if (nnsdk_log_level <= sdk_debug_level) \
    {__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "[%s:%d]" fmt, __FUNCTION__, __LINE__, ##args);}
#else

#define convert_loge(nnsdk_log_level, fmt, ... ) \
    nn_sdk_LogMsg(SDK_DEBUG_LEVEL_ERROR, "E %s[%s:%d]" fmt, CONVERT_LOG_TAG, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define convert_logw(nnsdk_log_level, fmt, ... ) \
    nn_sdk_LogMsg(SDK_DEBUG_LEVEL_WARN,  "W %s[%s:%d]" fmt, CONVERT_LOG_TAG, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define convert_logi(nnsdk_log_level, fmt, ... ) \
    nn_sdk_LogMsg(SDK_DEBUG_LEVEL_INFO,  "I %s[%s:%d]" fmt, CONVERT_LOG_TAG, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define convert_logp(nnsdk_log_level, fmt, ... ) \
    nn_sdk_LogMsg(SDK_DEBUG_LEVEL_PROCESS, "P %s[%s:%d]" fmt, CONVERT_LOG_TAG, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define convert_logd(nnsdk_log_level, fmt, ... ) \
    nn_sdk_LogMsg(SDK_DEBUG_LEVEL_DEBUG, "D %s[%s:%d]" fmt, CONVERT_LOG_TAG, __FUNCTION__, __LINE__, ##__VA_ARGS__)


#define LOGE(nnsdk_log_level, fmt, ... ) \
    nn_sdk_LogMsg(SDK_DEBUG_LEVEL_ERROR, "E %s[%s:%d]" fmt, LOG_TAG, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define LOGW(nnsdk_log_level, fmt, ... ) \
    nn_sdk_LogMsg(SDK_DEBUG_LEVEL_WARN,  "W %s[%s:%d]" fmt, LOG_TAG, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define LOGI(nnsdk_log_level, fmt, ... ) \
    nn_sdk_LogMsg(SDK_DEBUG_LEVEL_INFO,  "I %s[%s:%d]" fmt, LOG_TAG, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define LOGP(nnsdk_log_level, fmt, ... ) \
    nn_sdk_LogMsg(SDK_DEBUG_LEVEL_PROCESS, "P %s[%s:%d]" fmt, LOG_TAG, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define LOGD(nnsdk_log_level, fmt, ... ) \
    nn_sdk_LogMsg(SDK_DEBUG_LEVEL_DEBUG, "D %s[%s:%d]" fmt, LOG_TAG, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define __LOG__(nnsdk_log_level, fmt, ... ) \
    nn_sdk_LogMsg(SDK_DEBUG_LEVEL_DEBUG, "%s[%s:%d]" fmt, LOG_TAG, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#endif

void nn_sdk_LogMsg(sdk_debug_level_t level, const char *fmt, ...);
void det_set_log_level(sdk_debug_level_t level,sdk_log_format_t output_format);

#ifdef __cplusplus
}
#endif
#endif