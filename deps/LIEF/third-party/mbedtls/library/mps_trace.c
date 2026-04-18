/*
 *  Message Processing Stack, Trace module
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_SSL_PROTO_TLS1_3)

#include "mps_common.h"

#if defined(MBEDTLS_MPS_ENABLE_TRACE)

#include "mps_trace.h"
#include <stdarg.h>

static int trace_depth = 0;

#define color_default  "\x1B[0m"
#define color_red      "\x1B[1;31m"
#define color_green    "\x1B[1;32m"
#define color_yellow   "\x1B[1;33m"
#define color_blue     "\x1B[1;34m"
#define color_magenta  "\x1B[1;35m"
#define color_cyan     "\x1B[1;36m"
#define color_white    "\x1B[1;37m"

static char const *colors[] =
{
    color_default,
    color_green,
    color_yellow,
    color_magenta,
    color_cyan,
    color_blue,
    color_white
};

#define MPS_TRACE_BUF_SIZE 100

void mbedtls_mps_trace_print_msg(int id, int line, const char *format, ...)
{
    int ret;
    char str[MPS_TRACE_BUF_SIZE];
    va_list argp;
    va_start(argp, format);
    ret = mbedtls_vsnprintf(str, MPS_TRACE_BUF_SIZE, format, argp);
    va_end(argp);

    if (ret >= 0 && ret < MPS_TRACE_BUF_SIZE) {
        str[ret] = '\0';
        mbedtls_printf("[%d|L%d]: %s\n", id, line, str);
    }
}

int mbedtls_mps_trace_get_depth()
{
    return trace_depth;
}
void mbedtls_mps_trace_dec_depth()
{
    trace_depth--;
}
void mbedtls_mps_trace_inc_depth()
{
    trace_depth++;
}

void mbedtls_mps_trace_color(int id)
{
    if (id > (int) (sizeof(colors) / sizeof(*colors))) {
        return;
    }
    printf("%s", colors[id]);
}

void mbedtls_mps_trace_indent(int level, mbedtls_mps_trace_type ty)
{
    if (level > 0) {
        while (--level) {
            printf("|  ");
        }

        printf("|  ");
    }

    switch (ty) {
        case MBEDTLS_MPS_TRACE_TYPE_COMMENT:
            mbedtls_printf("@ ");
            break;

        case MBEDTLS_MPS_TRACE_TYPE_CALL:
            mbedtls_printf("+--> ");
            break;

        case MBEDTLS_MPS_TRACE_TYPE_ERROR:
            mbedtls_printf("E ");
            break;

        case MBEDTLS_MPS_TRACE_TYPE_RETURN:
            mbedtls_printf("< ");
            break;

        default:
            break;
    }
}

#endif /* MBEDTLS_MPS_ENABLE_TRACE */
#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */
