#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "aml_log.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#define DEBUG_BUFFER_LEN 1280


static int _check_log_level(const char *module_name, aml_debug_level_t level)
{
    aml_debug_level_t env_log_level;

    char env_name[256] = {0};
    snprintf(env_name, 256-1, "%s_LOG_LEVEL", module_name);
    char* log_level = getenv(env_name);

    if (log_level) {
        env_log_level = (aml_debug_level_t)atoi(log_level);
    }
    else
    {
        env_log_level = AML_DEBUG_LEVEL_WARN;
    }

    if (env_log_level >= level) {
        return 1;
    }
    return 0;
}

void aml_LogMsg(const char *module_name, aml_debug_level_t level, const char *fmt, ...)
{
    char arg_buffer[DEBUG_BUFFER_LEN] = {0};
    va_list arg;

    if (_check_log_level(module_name, level) == 0) {
        return ;
    }

    va_start(arg, fmt);
    vsnprintf(arg_buffer, DEBUG_BUFFER_LEN, fmt, arg);
    va_end(arg);

    fprintf(stderr, "%s\n", arg_buffer);
} /* dma_LogMsg() */
