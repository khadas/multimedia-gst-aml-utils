#ifndef __AML_LOG__
#define __AML_LOG__

#ifdef __cplusplus
extern "C"{
#endif


//#define AML_LOG_MODULE_NAME “xxx”

#define MODULE_NAME AML_LOG_MODULE_NAME

///@brief debug level
typedef enum{
	AML_DEBUG_LEVEL_DEFAULT = 0,	///< close debug
	AML_DEBUG_LEVEL_ERROR,		    ///< error level,hightest level system will exit and crash
	AML_DEBUG_LEVEL_WARN,		    ///< warning, system continue working,but something maybe wrong
	AML_DEBUG_LEVEL_INFO,		    ///< info some value if needed
	AML_DEBUG_LEVEL_PROCESS,	    ///< default,some process print
	AML_DEBUG_LEVEL_DEBUG		    ///< debug level,just for debug
}aml_debug_level_t;


#define LOG_Default( fmt, ... ) \
    aml_LogMsg(MODULE_NAME, AML_DEBUG_LEVEL_DEFAULT, "Default %s[%s:%d]" fmt, MODULE_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define LOGE( fmt, ... ) \
    aml_LogMsg(MODULE_NAME, AML_DEBUG_LEVEL_ERROR, "E %s[%s:%d]" fmt, MODULE_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGW( fmt, ... ) \
    aml_LogMsg(MODULE_NAME, AML_DEBUG_LEVEL_WARN,  "W %s[%s:%d]" fmt, MODULE_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGI( fmt, ... ) \
    aml_LogMsg(MODULE_NAME, AML_DEBUG_LEVEL_INFO,  "I %s[%s:%d]" fmt, MODULE_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGP( fmt, ... ) \
    aml_LogMsg(MODULE_NAME, AML_DEBUG_LEVEL_PROCESS, "P %s[%s:%d]" fmt, MODULE_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGD( fmt, ... ) \
    aml_LogMsg(MODULE_NAME, AML_DEBUG_LEVEL_DEBUG, "D %s[%s:%d]" fmt, MODULE_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define __LOG__( fmt, ... ) \
    aml_LogMsg(MODULE_NAME, AML_DEBUG_LEVEL_DEBUG, "%s[%s:%d]" fmt, MODULE_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)

void aml_LogMsg(const char *module_name, aml_debug_level_t level, const char *fmt, ...);


#ifdef __cplusplus
}
#endif
#endif
