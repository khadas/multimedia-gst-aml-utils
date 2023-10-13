#ifndef __DMA_LOG__
#define __DMA_LOG__

#ifdef __cplusplus
extern "C"{
#endif

#define API_NAME  "DMA_API:"



///@brief log output format
typedef enum{
  DMA_LOG_NULL = -1,	        ///< close all log output
  DMA_LOG_TERMINAL,	            ///< set log print to terminal
  DMA_LOG_FILE,		            ///< set log print to file
  DMA_LOG_SYSTEM                ///< set log print to system
}dma_log_format_t;

///@brief debug level
typedef enum{
	DMA_DEBUG_LEVEL_RELEASE = -1,	///< close debug
	DMA_DEBUG_LEVEL_ERROR,		    ///< error level,hightest level system will exit and crash
	DMA_DEBUG_LEVEL_WARN,		    ///< warning, system continue working,but something maybe wrong
	DMA_DEBUG_LEVEL_INFO,		    ///< info some value if needed
	DMA_DEBUG_LEVEL_PROCESS,	    ///< default,some process print
	DMA_DEBUG_LEVEL_DEBUG		    ///< debug level,just for debug
}dma_debug_level_t;


#define LOG_Default( fmt, ... ) \
    dma_LogMsg(DMA_DEBUG_LEVEL_RELEASE, "Default %s[%s:%d]" fmt, API_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define LOGE( fmt, ... ) \
    dma_LogMsg(DMA_DEBUG_LEVEL_ERROR, "E %s[%s:%d]" fmt, API_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGW( fmt, ... ) \
    dma_LogMsg(DMA_DEBUG_LEVEL_WARN,  "W %s[%s:%d]" fmt, API_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGI( fmt, ... ) \
    dma_LogMsg(DMA_DEBUG_LEVEL_INFO,  "I %s[%s:%d]" fmt, API_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGP( fmt, ... ) \
    dma_LogMsg(DMA_DEBUG_LEVEL_PROCESS, "D %s[%s:%d]" fmt, API_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGD( fmt, ... ) \
    dma_LogMsg(DMA_DEBUG_LEVEL_DEBUG, "D %s[%s:%d]" fmt, API_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define __LOG__( fmt, ... ) \
    dma_LogMsg(DMA_DEBUG_LEVEL_DEBUG, "%s[%s:%d]" fmt, API_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)

void dma_LogMsg(dma_debug_level_t level, const char *fmt, ...);
void dma_set_log_level(dma_debug_level_t level,dma_log_format_t output_format);


#ifdef __cplusplus
}
#endif
#endif
