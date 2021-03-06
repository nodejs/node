/*
 * Copyright 2016-2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "../testutil.h"
#include "internal/nelem.h"
#include "output.h"
#include "tu_local.h"

#include <string.h>

static size_t arg_count;
static char **args;
static unsigned char arg_used[1000];

static void check_arg_usage(void)
{
    size_t i, n = arg_count < OSSL_NELEM(arg_used) ? arg_count
                                                   : OSSL_NELEM(arg_used);

    for (i = 0; i < n; i++)
        if (!arg_used[i+1])
            test_printf_stderr("Warning ignored command-line argument %d: %s\n",
                               i, args[i+1]);
    if (i < arg_count)
        test_printf_stderr("Warning arguments %zu and later unchecked\n", i);
}

int main(int argc, char *argv[])
{
    int ret = EXIT_FAILURE;

    test_open_streams();

    if (!global_init()) {
        test_printf_stderr("Global init failed - aborting\n");
        return ret;
    }

    arg_count = argc - 1;
    args = argv;

    setup_test_framework();

    if (setup_tests())
        ret = run_tests(argv[0]);
    cleanup_tests();
    check_arg_usage();

    ret = pulldown_test_framework(ret);
    test_close_streams();
    return ret;
}

const char *test_get_program_name(void)
{
    return args[0];
}

char *test_get_argument(size_t n)
{
    if (n > arg_count)
        return NULL;
    if (n + 1 < OSSL_NELEM(arg_used))
        arg_used[n + 1] = 1;
    return args[n + 1];
}

size_t test_get_argument_count(void)
{
    return arg_count;
}

int test_has_option(const char *option)
{
    size_t i;

    for (i = 1; i <= arg_count; i++)
        if (strcmp(args[i], option) == 0) {
            arg_used[i] = 1;
            return 1;
        }
    return 0;
}

const char *test_get_option_argument(const char *option)
{
    size_t i, n = strlen(option);

    for (i = 1; i <= arg_count; i++)
        if (strncmp(args[i], option, n) == 0) {
            arg_used[i] = 1;
            if (args[i][n] == '\0' && i + 1 < arg_count) {
                arg_used[++i] = 1;
                return args[i];
            }
            return args[i] + n;
        }
    return NULL;
}

