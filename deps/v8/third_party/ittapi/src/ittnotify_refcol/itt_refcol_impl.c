#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INTEL_NO_MACRO_BODY
#define INTEL_ITTNOTIFY_API_PRIVATE
#include "ittnotify.h"
#include "ittnotify_config.h"

#define LOG_BUFFER_MAX_SIZE 256

static const char* env_log_dir = "INTEL_LIBITTNOTIFY_LOG_DIR";
static const char* log_level_str[] = {"INFO", "WARN", "ERROR", "FATAL_ERROR"};

enum {
    LOG_LVL_INFO,
    LOG_LVL_WARN,
    LOG_LVL_ERROR,
    LOG_LVL_FATAL
};

static struct ref_collector_logger {
    char* file_name;
    uint8_t init_state;
} g_ref_collector_logger = {NULL, 0};

char* log_file_name_generate()
{
    time_t time_now = time(NULL);
    struct tm* time_info = localtime(&time_now);
    char* log_file_name = malloc(sizeof(char) * (LOG_BUFFER_MAX_SIZE/2));

    sprintf(log_file_name,"libittnotify_refcol_%d%d%d%d%d%d.log",
            time_info->tm_year+1900, time_info->tm_mon+1, time_info->tm_mday,
            time_info->tm_hour, time_info->tm_min, time_info->tm_sec);

    return log_file_name;
}

void ref_col_init()
{
    if (!g_ref_collector_logger.init_state)
    {
        static char file_name_buffer[LOG_BUFFER_MAX_SIZE*2];
        char* log_dir = getenv(env_log_dir);
        char* log_file = log_file_name_generate();

        if (log_dir != NULL)
        {
            #ifdef _WIN32
                sprintf(file_name_buffer,"%s\\%s", log_dir, log_file);
            #else
                sprintf(file_name_buffer,"%s/%s", log_dir, log_file);
            #endif
        }
        else
        {
            #ifdef _WIN32
                char* temp_dir = getenv("TEMP");
                if (temp_dir != NULL)
                {
                    sprintf(file_name_buffer,"%s\\%s", temp_dir, log_file);
                }
            #else
                sprintf(file_name_buffer,"/tmp/%s", log_file);
            #endif
        }

        g_ref_collector_logger.file_name = file_name_buffer;
        g_ref_collector_logger.init_state = 1;
    }
}

static void fill_func_ptr_per_lib(__itt_global* p)
{
    __itt_api_info* api_list = (__itt_api_info*)p->api_list_ptr;

    for (int i = 0; api_list[i].name != NULL; i++)
    {
        *(api_list[i].func_ptr) = (void*)__itt_get_proc(p->lib, api_list[i].name);
        if (*(api_list[i].func_ptr) == NULL)
        {
            *(api_list[i].func_ptr) = api_list[i].null_func;
        }
    }
}

ITT_EXTERN_C void ITTAPI __itt_api_init(__itt_global* p, __itt_group_id init_groups)
{
    if (p != NULL)
    {
        fill_func_ptr_per_lib(p);
        ref_col_init();
    }
    else
    {
        printf("ERROR: Failed to initialize dynamic library\n");
    }
}

void log_func_call(uint8_t log_level, const char* function_name, const char* message_format, ...)
{
    if (g_ref_collector_logger.init_state)
    {
        FILE * pLogFile = NULL;
        char log_buffer[LOG_BUFFER_MAX_SIZE];
        uint32_t result_len = 0;
        va_list message_args;

        result_len += sprintf(log_buffer, "[%s] %s(...) - ", log_level_str[log_level] ,function_name);
        va_start(message_args, message_format);
        vsnprintf(log_buffer + result_len, LOG_BUFFER_MAX_SIZE - result_len, message_format, message_args);

        pLogFile = fopen(g_ref_collector_logger.file_name, "a");
        if (!pLogFile)
        {
            printf("ERROR: Cannot open file: %s\n", g_ref_collector_logger.file_name);
            return;
        }
        fprintf(pLogFile, "%s\n", log_buffer);
        fclose(pLogFile);
    }
    else
    {
        return;
    }
}

#define LOG_FUNC_CALL_INFO(...)  log_func_call(LOG_LVL_INFO, __FUNCTION__, __VA_ARGS__)
#define LOG_FUNC_CALL_WARN(...)  log_func_call(LOG_LVL_WARN, __FUNCTION__, __VA_ARGS__)
#define LOG_FUNC_CALL_ERROR(...) log_func_call(LOG_LVL_ERROR, __FUNCTION__, __VA_ARGS__)
#define LOG_FUNC_CALL_FATAL(...) log_func_call(LOG_LVL_FATAL, __FUNCTION__, __VA_ARGS__)

/* ------------------------------------------------------------------------------ */
/* The code below is a reference implementation of the ITT API dynamic collector. */
/* This implementation is designed to log ITTAPI functions calls.*/
/* ------------------------------------------------------------------------------ */

char* get_metadata_elements(size_t size, __itt_metadata_type type, void* metadata)
{
    char* metadata_str = malloc(sizeof(char) * LOG_BUFFER_MAX_SIZE);
    *metadata_str = '\0';

    switch (type)
    {
    case __itt_metadata_u64:
        for (uint16_t i = 0; i < size; i++)
            sprintf(metadata_str, "%s%llu;", metadata_str, ((uint64_t*)metadata)[i]);
        break;
    case __itt_metadata_s64:
        for (uint16_t i = 0; i < size; i++)
            sprintf(metadata_str, "%s%lld;", metadata_str, ((int64_t*)metadata)[i]);
        break;
    case __itt_metadata_u32:
        for (uint16_t i = 0; i < size; i++)
            sprintf(metadata_str, "%s%lu;", metadata_str, ((uint32_t*)metadata)[i]);
        break;
    case __itt_metadata_s32:
        for (uint16_t i = 0; i < size; i++)
            sprintf(metadata_str, "%s%ld;", metadata_str, ((int32_t*)metadata)[i]);
        break;
    case __itt_metadata_u16:
        for (uint16_t i = 0; i < size; i++)
            sprintf(metadata_str, "%s%u;", metadata_str, ((uint16_t*)metadata)[i]);
        break;
    case __itt_metadata_s16:
        for (uint16_t i = 0; i < size; i++)
            sprintf(metadata_str, "%s%d;", metadata_str, ((int16_t*)metadata)[i]);
        break;
    case __itt_metadata_float:
        for (uint16_t i = 0; i < size; i++)
            sprintf(metadata_str, "%s%f;", metadata_str, ((float*)metadata)[i]);
        break;
    case __itt_metadata_double:
        for (uint16_t i = 0; i < size; i++)
            sprintf(metadata_str, "%s%lf;", metadata_str, ((double*)metadata)[i]);
        break;
    default:
            printf("ERROR: Unknow metadata type\n");
    }

    return metadata_str;
}

ITT_EXTERN_C void ITTAPI __itt_pause(void)
{
    LOG_FUNC_CALL_INFO("function call");
}

ITT_EXTERN_C void ITTAPI __itt_resume(void)
{
    LOG_FUNC_CALL_INFO("function call");
}

ITT_EXTERN_C void ITTAPI __itt_detach(void)
{
    LOG_FUNC_CALL_INFO("function call");
}

ITT_EXTERN_C void ITTAPI __itt_frame_begin_v3(const __itt_domain *domain, __itt_id *id)
{
    if (domain != NULL)
    {
        LOG_FUNC_CALL_INFO("functions args: domain_name=%s", domain->nameA);
    }
    else
    {
        LOG_FUNC_CALL_WARN("Incorrect function call");
    }
}

ITT_EXTERN_C void ITTAPI __itt_frame_end_v3(const __itt_domain *domain, __itt_id *id)
{
    if (domain != NULL)
    {
        LOG_FUNC_CALL_INFO("functions args: domain_name=%s", domain->nameA);
    }
    else
    {
        LOG_FUNC_CALL_WARN("Incorrect function call");
    }
}

ITT_EXTERN_C void ITTAPI __itt_frame_submit_v3(const __itt_domain *domain, __itt_id *id,
    __itt_timestamp begin, __itt_timestamp end)
{
    if (domain != NULL)
    {
        LOG_FUNC_CALL_INFO("functions args: domain_name=%s, time_begin=%lu, time_end=%llu",
                        domain->nameA, (uint64_t*)id, begin, end);
    }
    else
    {
        LOG_FUNC_CALL_WARN("Incorrect function call");
    }
}

ITT_EXTERN_C void ITTAPI __itt_task_begin(
    const __itt_domain *domain, __itt_id taskid, __itt_id parentid, __itt_string_handle *name)
{
    if (domain != NULL && name != NULL)
    {
        LOG_FUNC_CALL_INFO("functions args: domain_name=%s handle_name=%s", domain->nameA, name->strA);
    }
    else
    {
        LOG_FUNC_CALL_WARN("Incorrect function call");
    }
}

ITT_EXTERN_C void ITTAPI __itt_task_end(const __itt_domain *domain)
{
    if (domain != NULL)
    {
        LOG_FUNC_CALL_INFO("functions args: domain_name=%s", domain->nameA);
    }
    else
    {
        LOG_FUNC_CALL_WARN("Incorrect function call");
    }
}

ITT_EXTERN_C void __itt_metadata_add(const __itt_domain *domain, __itt_id id,
    __itt_string_handle *key, __itt_metadata_type type, size_t count, void *data)
{
    if (domain != NULL && count != 0)
    {
        LOG_FUNC_CALL_INFO("functions args: domain_name=%s metadata_size=%lu metadata[]=%s",
                            domain->nameA, count, get_metadata_elements(count, type, data));
    }
    else
    {
        LOG_FUNC_CALL_WARN("Incorrect function call");
    }
}
