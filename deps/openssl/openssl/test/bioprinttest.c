/*
 * Copyright 2016-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#define TESTUTIL_NO_size_t_COMPARISON

#include <inttypes.h>
#if defined(__TANDEM) && defined(__H_Series_RVU)
/* Restrict this block to NonStop J-series (Itanium) only. */
# if defined(__LP64)
#  define PRIdPTR                  "lld"
#  define PRIiPTR                  "lli"
#  define PRIoPTR                  "llo"
#  define PRIuPTR                  "llu"
#  define PRIxPTR                  "llx"
#  define PRIXPTR                  "llX"
# else
#  define PRIdPTR                  "d"
#  define PRIiPTR                  "i"
#  define PRIoPTR                  "o"
#  define PRIuPTR                  "u"
#  define PRIxPTR                  "x"
#  define PRIXPTR                  "X"
# endif
#endif
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <openssl/bio.h>
#include "internal/nelem.h"
#include "internal/numbers.h"
#include "internal/bio.h"
#include "testutil.h"
#include "testutil/output.h"

static int justprint = 0;

static const char * const fpexpected[][11][5] = {
    {
        /*  0.00 */ { "0.0000e+00", "0.0000", "0", "0.0000E+00", "0" },
        /*  0.01 */ { "6.7000e-01", "0.6700", "0.67", "6.7000E-01", "0.67" },
        /*  0.02 */ { "6.6667e-01", "0.6667", "0.6667", "6.6667E-01", "0.6667" },
        /*  0.03 */ { "6.6667e-04", "0.0007", "0.0006667", "6.6667E-04", "0.0006667" },
        /*  0.04 */ { "6.6667e-05", "0.0001", "6.667e-05", "6.6667E-05", "6.667E-05" },
        /*  0.05 */ { "6.6667e+00", "6.6667", "6.667", "6.6667E+00", "6.667" },
        /*  0.06 */ { "6.6667e+01", "66.6667", "66.67", "6.6667E+01", "66.67" },
        /*  0.07 */ { "6.6667e+02", "666.6667", "666.7", "6.6667E+02", "666.7" },
        /*  0.08 */ { "6.6667e+03", "6666.6667", "6667", "6.6667E+03", "6667" },
        /*  0.09 */ { "6.6667e+04", "66666.6667", "6.667e+04", "6.6667E+04", "6.667E+04" },
        /*  0.10 */ { "-6.6667e+04", "-66666.6667", "-6.667e+04", "-6.6667E+04", "-6.667E+04" },
    },
    {
        /*  1.00 */ { "0.00000e+00", "0.00000", "0", "0.00000E+00", "0" },
        /*  1.01 */ { "6.70000e-01", "0.67000", "0.67", "6.70000E-01", "0.67" },
        /*  1.02 */ { "6.66667e-01", "0.66667", "0.66667", "6.66667E-01", "0.66667" },
        /*  1.03 */ { "6.66667e-04", "0.00067", "0.00066667", "6.66667E-04", "0.00066667" },
        /*  1.04 */ { "6.66667e-05", "0.00007", "6.6667e-05", "6.66667E-05", "6.6667E-05" },
        /*  1.05 */ { "6.66667e+00", "6.66667", "6.6667", "6.66667E+00", "6.6667" },
        /*  1.06 */ { "6.66667e+01", "66.66667", "66.667", "6.66667E+01", "66.667" },
        /*  1.07 */ { "6.66667e+02", "666.66667", "666.67", "6.66667E+02", "666.67" },
        /*  1.08 */ { "6.66667e+03", "6666.66667", "6666.7", "6.66667E+03", "6666.7" },
        /*  1.09 */ { "6.66667e+04", "66666.66667", "66667", "6.66667E+04", "66667" },
        /*  1.10 */ { "-6.66667e+04", "-66666.66667", "-66667", "-6.66667E+04", "-66667" },
    },
    {
        /*  2.00 */ { "  0.0000e+00", "      0.0000", "           0", "  0.0000E+00", "           0" },
        /*  2.01 */ { "  6.7000e-01", "      0.6700", "        0.67", "  6.7000E-01", "        0.67" },
        /*  2.02 */ { "  6.6667e-01", "      0.6667", "      0.6667", "  6.6667E-01", "      0.6667" },
        /*  2.03 */ { "  6.6667e-04", "      0.0007", "   0.0006667", "  6.6667E-04", "   0.0006667" },
        /*  2.04 */ { "  6.6667e-05", "      0.0001", "   6.667e-05", "  6.6667E-05", "   6.667E-05" },
        /*  2.05 */ { "  6.6667e+00", "      6.6667", "       6.667", "  6.6667E+00", "       6.667" },
        /*  2.06 */ { "  6.6667e+01", "     66.6667", "       66.67", "  6.6667E+01", "       66.67" },
        /*  2.07 */ { "  6.6667e+02", "    666.6667", "       666.7", "  6.6667E+02", "       666.7" },
        /*  2.08 */ { "  6.6667e+03", "   6666.6667", "        6667", "  6.6667E+03", "        6667" },
        /*  2.09 */ { "  6.6667e+04", "  66666.6667", "   6.667e+04", "  6.6667E+04", "   6.667E+04" },
        /*  2.10 */ { " -6.6667e+04", " -66666.6667", "  -6.667e+04", " -6.6667E+04", "  -6.667E+04" },
    },
    {
        /*  3.00 */ { " 0.00000e+00", "     0.00000", "           0", " 0.00000E+00", "           0" },
        /*  3.01 */ { " 6.70000e-01", "     0.67000", "        0.67", " 6.70000E-01", "        0.67" },
        /*  3.02 */ { " 6.66667e-01", "     0.66667", "     0.66667", " 6.66667E-01", "     0.66667" },
        /*  3.03 */ { " 6.66667e-04", "     0.00067", "  0.00066667", " 6.66667E-04", "  0.00066667" },
        /*  3.04 */ { " 6.66667e-05", "     0.00007", "  6.6667e-05", " 6.66667E-05", "  6.6667E-05" },
        /*  3.05 */ { " 6.66667e+00", "     6.66667", "      6.6667", " 6.66667E+00", "      6.6667" },
        /*  3.06 */ { " 6.66667e+01", "    66.66667", "      66.667", " 6.66667E+01", "      66.667" },
        /*  3.07 */ { " 6.66667e+02", "   666.66667", "      666.67", " 6.66667E+02", "      666.67" },
        /*  3.08 */ { " 6.66667e+03", "  6666.66667", "      6666.7", " 6.66667E+03", "      6666.7" },
        /*  3.09 */ { " 6.66667e+04", " 66666.66667", "       66667", " 6.66667E+04", "       66667" },
        /*  3.10 */ { "-6.66667e+04", "-66666.66667", "      -66667", "-6.66667E+04", "      -66667" },
    },
    {
        /*  4.00 */ { "0e+00", "0", "0", "0E+00", "0" },
        /*  4.01 */ { "7e-01", "1", "0.7", "7E-01", "0.7" },
        /*  4.02 */ { "7e-01", "1", "0.7", "7E-01", "0.7" },
        /*  4.03 */ { "7e-04", "0", "0.0007", "7E-04", "0.0007" },
        /*  4.04 */ { "7e-05", "0", "7e-05", "7E-05", "7E-05" },
        /*  4.05 */ { "7e+00", "7", "7", "7E+00", "7" },
        /*  4.06 */ { "7e+01", "67", "7e+01", "7E+01", "7E+01" },
        /*  4.07 */ { "7e+02", "667", "7e+02", "7E+02", "7E+02" },
        /*  4.08 */ { "7e+03", "6667", "7e+03", "7E+03", "7E+03" },
        /*  4.09 */ { "7e+04", "66667", "7e+04", "7E+04", "7E+04" },
        /*  4.10 */ { "-7e+04", "-66667", "-7e+04", "-7E+04", "-7E+04" },
    },
    {
        /*  5.00 */ { "0.000000e+00", "0.000000", "0", "0.000000E+00", "0" },
        /*  5.01 */ { "6.700000e-01", "0.670000", "0.67", "6.700000E-01", "0.67" },
        /*  5.02 */ { "6.666667e-01", "0.666667", "0.666667", "6.666667E-01", "0.666667" },
        /*  5.03 */ { "6.666667e-04", "0.000667", "0.000666667", "6.666667E-04", "0.000666667" },
        /*  5.04 */ { "6.666667e-05", "0.000067", "6.66667e-05", "6.666667E-05", "6.66667E-05" },
        /*  5.05 */ { "6.666667e+00", "6.666667", "6.66667", "6.666667E+00", "6.66667" },
        /*  5.06 */ { "6.666667e+01", "66.666667", "66.6667", "6.666667E+01", "66.6667" },
        /*  5.07 */ { "6.666667e+02", "666.666667", "666.667", "6.666667E+02", "666.667" },
        /*  5.08 */ { "6.666667e+03", "6666.666667", "6666.67", "6.666667E+03", "6666.67" },
        /*  5.09 */ { "6.666667e+04", "66666.666667", "66666.7", "6.666667E+04", "66666.7" },
        /*  5.10 */ { "-6.666667e+04", "-66666.666667", "-66666.7", "-6.666667E+04", "-66666.7" },
    },
    {
        /*  6.00 */ { "0.0000e+00", "000.0000", "00000000", "0.0000E+00", "00000000" },
        /*  6.01 */ { "6.7000e-01", "000.6700", "00000.67", "6.7000E-01", "00000.67" },
        /*  6.02 */ { "6.6667e-01", "000.6667", "000.6667", "6.6667E-01", "000.6667" },
        /*  6.03 */ { "6.6667e-04", "000.0007", "0.0006667", "6.6667E-04", "0.0006667" },
        /*  6.04 */ { "6.6667e-05", "000.0001", "6.667e-05", "6.6667E-05", "6.667E-05" },
        /*  6.05 */ { "6.6667e+00", "006.6667", "0006.667", "6.6667E+00", "0006.667" },
        /*  6.06 */ { "6.6667e+01", "066.6667", "00066.67", "6.6667E+01", "00066.67" },
        /*  6.07 */ { "6.6667e+02", "666.6667", "000666.7", "6.6667E+02", "000666.7" },
        /*  6.08 */ { "6.6667e+03", "6666.6667", "00006667", "6.6667E+03", "00006667" },
        /*  6.09 */ { "6.6667e+04", "66666.6667", "6.667e+04", "6.6667E+04", "6.667E+04" },
        /*  6.10 */ { "-6.6667e+04", "-66666.6667", "-6.667e+04", "-6.6667E+04", "-6.667E+04" },
    },
};

static int(*test_BIO_snprintf)(char *, size_t, const char *, ...) = BIO_snprintf;

enum arg_type {
    AT_NONE = 0,
    AT_CHAR, AT_SHORT, AT_INT, AT_LONG, AT_LLONG,
    /* The ones below are used in n_data only so far */
    AT_SIZE, AT_PTRDIFF, AT_STR,
};

static const struct int_data {
    union {
        unsigned char hh;
        unsigned short h;
        unsigned int i;
        unsigned long l;
        unsigned long long ll;
    } value;
    enum arg_type type;
    const char *format;
    const char *expected;
    bool skip_libc_check;
    /* Since OpenSSL's snprintf is non-standard on buffer overflow */
    bool skip_libc_ret_check;
    int exp_ret;
} int_data[] = {
    { { .hh = 0x42 }, AT_CHAR, "%+hhu", "66" },
    { { .hh = 0x88 }, AT_CHAR, "%hhd", "-120" },
    { { .hh = 0x0 }, AT_CHAR, "%hho", "0" },
    { { .hh = 0x0 }, AT_CHAR, "%#hho", "0" },
    { { .hh = 0x1 }, AT_CHAR, "%hho", "1" },
    { { .hh = 0x1 }, AT_CHAR, "%#hho", "01" },
    { { .hh = 0x0 }, AT_CHAR, "%+hhx", "0" },
    { { .hh = 0x0 }, AT_CHAR, "%#hhx", "0" },
    { { .hh = 0xf }, AT_CHAR, "%hhx", "f" },
    { { .hh = 0xe }, AT_CHAR, "%hhX", "E" },
    { { .hh = 0xd }, AT_CHAR, "%#hhx", "0xd" },
    { { .hh = 0xc }, AT_CHAR, "%#hhX", "0XC" },
    { { .hh = 0xb }, AT_CHAR, "%#04hhX", "0X0B" },
    { { .hh = 0xa }, AT_CHAR, "%#-015hhx", "0xa            " },
    { { .hh = 0x9 }, AT_CHAR, "%#+01hho", "011" },
    { { .hh = 0x8 }, AT_CHAR, "%#09hho", "000000010" },
    { { .hh = 0x7 }, AT_CHAR, "%#+ 9hhi", "       +7" },
    { { .hh = 0x6 }, AT_CHAR, "%# 9hhd", "        6" },
    { { .hh = 0x95 }, AT_CHAR, "%#06hhi", "-00107" },
    { { .hh = 0x4 }, AT_CHAR, "%# hhd", " 4" },
    { { .hh = 0x3 }, AT_CHAR, "%# hhu", "3" },
    { { .hh = 0x0 }, AT_CHAR, "%02hhx", "00" },
    { { .h = 0 }, AT_SHORT, "|%.0hd|", "||" },
    { { .h = 0 }, AT_SHORT, "|%.hu|", "||" },
#if !defined(__OpenBSD__)
    { { .h = 0 }, AT_SHORT, "|%#.ho|", "|0|" },
#endif
    { { .h = 1 }, AT_SHORT, "%4.2hi", "  01" },
    { { .h = 2 }, AT_SHORT, "%-4.3hu", "002 " },
    { { .h = 3 }, AT_SHORT, "%+.3hu", "003" },
    { { .h = 9 }, AT_SHORT, "%#5.2ho", "  011" },
    { { .h = 0xf }, AT_SHORT, "%#-6.2hx", "0x0f  " },
    { { .h = 0xaa }, AT_SHORT, "%#8.0hX", "    0XAA" },
    { { .h = 0xdead }, AT_SHORT, "%#hi", "-8531" },
    { { .h = 0xcafe }, AT_SHORT, "%#0.1hX", "0XCAFE" },
    { { .h = 0xbeef }, AT_SHORT, "%#012.8ho", "    00137357" },
    { { .h = 0xbeef }, AT_SHORT, "%#000000000000000000012.ho", "     0137357" },
    { { .h = 0xbeef }, AT_SHORT, "%012.ho", "      137357" },
    { { .h = 0xfade }, AT_SHORT, "%#012ho", "000000175336" },
    { { .h = 0xfaff }, AT_SHORT, "%#-012ho", "0175377     " },
    { { .h = 0xbea7 }, AT_SHORT, "%#-012.8ho", "00137247    " },
    { { .i = 0 }, AT_INT, "-%#+.0u-", "--" },
    { { .i = 0 }, AT_INT, "-%-8.u-", "-        -" },
    { { .i = 0xdeadc0de }, AT_INT, "%#+67.65i",
      " -0000000000000000000000000000000000000000000000000000000055903",
      .skip_libc_ret_check = true, .exp_ret = -1 },
    { { .i = 0xfeedface }, AT_INT, "%#+70.10X",
      "                                                          0X00F",
      .skip_libc_ret_check = true, .exp_ret = -1 },
    { { .i = 0xdecaffee }, AT_INT, "%76.15o",
      "                                                             00",
      .skip_libc_ret_check = true, .exp_ret = -1 },
    { { .i = 0x5ad }, AT_INT, "%#67.x",
      "                                                              0",
      .skip_libc_ret_check = true, .exp_ret = -1 },
    { { .i = 0x1337 }, AT_INT, "|%2147483639.x|",
      "|                                                              ",
      .skip_libc_check = true, .exp_ret = -1 },
#if !defined(OPENSSL_SYS_WINDOWS)
    /*
     * those test crash on x86 windows built by VS-2019
     */
    { { .i = 0x1337 }, AT_INT, "|%.2147483639x|",
      "|00000000000000000000000000000000000000000000000000000000000000",
      .skip_libc_check = true, .exp_ret = -1 },
    /*
     * We treat the following three format strings as errneous and bail out
     * mid-string.
     */
    { { .i = 0x1337 }, AT_INT, "|%2147483647.x|", "|",
      .skip_libc_check = true, .exp_ret = -1 },
#endif
    { { .i = 0x1337 }, AT_INT,
      "abcdefghijklmnopqrstuvwxyz0123456789ZYXWVUTSRQPONMLKJIHGFEDCBA"
      "|%4294967295.x|",
      "abcdefghijklmnopqrstuvwxyz0123456789ZYXWVUTSRQPONMLKJIHGFEDCBA|",
      .skip_libc_check = true, .exp_ret = -1 },
    { { .i = 0x1337 }, AT_INT, "%4294967302.x", "",
      .skip_libc_check = true, .exp_ret = -1 },
    { { .i = 0xbeeface }, AT_INT, "%#+-12.1d", "+200211150  " },
#if !defined(__OpenBSD__)
    { { .l = 0 }, AT_LONG, "%%%#.0lo%%", "%0%" },
#endif
    { { .l = 0 }, AT_LONG, "%%%.0lo%%", "%%" },
    { { .l = 0 }, AT_LONG, "%%%-.0lo%%", "%%" },
    { { .l = 0xfacefed }, AT_LONG, "%#-1.14ld", "00000262991853" },
    { { .l = 0xdefaced }, AT_LONG, "%#+-014.11li", "+00233811181  " },
    { { .l = 0xfacade }, AT_LONG, "%#0.14lo", "00000076545336" },
    { { .l = 0 }, AT_LONG, "%#0.14lo", "00000000000000" },
    { { .l = 0xfacade }, AT_LONG, "%#0.14lx", "0x00000000facade" },
    { { .ll = 0 }, AT_LLONG, "#%#.0llx#", "##" },
    { { .ll = 0 }, AT_LLONG, "#%.0llx#", "##" },
    { { .ll = 0xffffFFFFffffFFFFULL }, AT_LLONG, "%#-032llo",
      "01777777777777777777777         " },
    { { .ll = 0xbadc0deddeadfaceULL }, AT_LLONG, "%022lld",
      "-004982091772484257074" },
};

static int test_int(int i)
{
    char bio_buf[64];
    int bio_ret;
    const struct int_data *data = int_data + i;
    const int exp_ret = data->exp_ret ? data->exp_ret
                                      : (int) strlen(data->expected);

    memset(bio_buf, '@', sizeof(bio_buf));

    switch (data->type) {
#define DO_PRINT(field_)                                                     \
    do {                                                                     \
        bio_ret = test_BIO_snprintf(bio_buf, sizeof(bio_buf), data->format,  \
                                    data->value.field_);                     \
    } while (0)
    case AT_CHAR:
        DO_PRINT(hh);
        break;
    case AT_SHORT:
        DO_PRINT(h);
        break;
    case AT_INT:
        DO_PRINT(i);
        break;
    case AT_LONG:
        DO_PRINT(l);
        break;
    case AT_LLONG:
        DO_PRINT(ll);
        break;
    default:
        TEST_error("Unexpected arg type: %d", data->type);
        return 0;
#undef DO_PRINT
    }

    if (data->skip_libc_check) {
        if (strcmp(bio_buf, data->expected) != 0)
	    TEST_note("%s Result (%s) does not match (%s)", __func__,
                      bio_buf, data->expected);
    } else if (!TEST_str_eq(bio_buf, data->expected)
        + !TEST_int_eq(bio_ret, exp_ret)) {
        TEST_note("Format: \"%s\"", data->format);
        return 0;
    }

    return 1;
}

#ifdef _WIN32
static int test_int_win32(int i)
{
    int ret;

    test_BIO_snprintf = ossl_BIO_snprintf_msvc;
    ret = test_int(i);
    test_BIO_snprintf = BIO_snprintf;

    return ret;
}
#endif

union ptrint {
    uintptr_t i;
    const char *s;
};

static const struct wp_data {
    union ptrint value;
    const char *format;
    const char *expected;
    int num_args;
    int arg1;
    int arg2;
    bool skip_libc_check;
    /* Since OpenSSL's snprintf is non-standard on buffer overflow */
    bool skip_libc_ret_check;
    int exp_ret;
} wp_data[] = {
    /* Integer checks with width/precision provided via arguments */
    { { .i = 01234 }, "%#*" PRIoPTR, "       01234", 1, 12 },
    { { .i = 01234 }, "%#.*" PRIxPTR, "0x00000000029c", 1, 12 },

    { { .i = 0 }, "|%#*" PRIoPTR "|", "| 0|", 1, 2 },
    { { .i = 0 }, "|%#.*" PRIoPTR "|", "|00|", 1, 2 },
    { { .i = 0 }, "|%#.*" PRIoPTR "|", "|0|", 1, 1 },
#if !defined(__OpenBSD__)
    { { .i = 0 }, "|%#.*" PRIoPTR "|", "|0|", 1, 0 },
#endif
    { { .i = 0 }, "|%.*" PRIoPTR "|", "||", 1, 0 },
    { { .i = 0 }, "|%#.*" PRIoPTR "|", "|0|", 1, -12 },

    { { .i = 0 }, "|%#.*" PRIxPTR "|", "||", 1, 0 },
    { { .i = 0 }, "|%#.*" PRIxPTR "|", "|0|", 1, -12 },
    { { .i = 1 }, "|%#.*" PRIxPTR "|", "|0x1|", 1, -12 },

    { { .i = 0 }, "|%#*.*" PRIxPTR "|", "|            |", 2, 12, 0 },
    { { .i = 1234 }, "|%*.*" PRIuPTR "|", "|      001234|", 2, 12, 6 },

    /* FreeBSD's libc bails out on the following three */
    { { .i = 1337 }, "|%*" PRIuPTR "|",
      "|                                                              ",
      1, 2147483647, .skip_libc_check = true, .exp_ret = -1 },
#if !defined(OPENSSL_SYS_WINDOWS)
    { { .i = 1337 }, "|%.*" PRIuPTR "|",
      "|00000000000000000000000000000000000000000000000000000000000000",
      1, 2147483647, .skip_libc_check = true, .exp_ret = -1 },
    { { .i = 1337 }, "|%#*.*" PRIoPTR "|",
      "|                                                             0",
      2, 2147483647, 2147483586, .skip_libc_check = true, .exp_ret = -1 },
#endif

    /* String width/precision checks */
    { { .s = "01234" }, "%12s", "       01234" },
    { { .s = "01234" }, "%-12s", "01234       " },
    { { .s = "01234" }, "%.12s", "01234" },
    { { .s = "01234" }, "%.2s", "01" },

    { { .s = "abc" }, "%*s", "         abc", 1, 12 },
    { { .s = "abc" }, "%*s", "abc         ", 1, -12 },
    { { .s = "abc" }, "%-*s", "abc         ", 1, 12 },
    { { .s = "abc" }, "%-*s", "abc         ", 1, -12 },

    { { .s = "ABC" }, "%*.*s", "         ABC", 2, 12, 5 },
    { { .s = "ABC" }, "%*.*s", "AB          ", 2, -12, 2 },
    { { .s = "ABC" }, "%-*.*s", "ABC         ", 2, 12, -5 },
    { { .s = "ABC" }, "%-*.*s", "ABC         ", 2, -12, -2 },

    { { .s = "def" }, "%.*s", "def", 1, 12 },
    { { .s = "%%s0123456789" }, "%.*s", "%%s01", 1, 5 },
    { { .s = "9876543210" }, "|%-61s|",
      "|9876543210                                                   |" },
    { { .s = "0123456789" }, "|%62s|",
      "|                                                    0123456789",
      .skip_libc_ret_check = true, .exp_ret = -1 },

    { { .s = "DEF" }, "%-2147483639s",
      "DEF                                                            ",
      .skip_libc_check = true, .exp_ret = -1 },
    { { .s = "DEF" }, "%-2147483640s", "",
      .skip_libc_check = true, .exp_ret = -1 },
    { { .s = "DEF" }, "%*s",
      "                                                               ",
      1, 2147483647, .skip_libc_check = true, .exp_ret = -1 },
};

static int test_width_precision(int i)
{
    char bio_buf[64];
    char std_buf[64];
    int bio_ret;
    const struct wp_data *data = wp_data + i;
    const int exp_ret = data->exp_ret ? data->exp_ret
                                      : (int) strlen(data->expected);

    memset(bio_buf, '@', sizeof(bio_buf));
    memset(std_buf, '#', sizeof(std_buf));

    switch (data->num_args) {
    case 2:
        bio_ret = test_BIO_snprintf(bio_buf, sizeof(bio_buf), data->format,
                                    data->arg1, data->arg2, data->value.i);
        break;

    case 1:
        bio_ret = test_BIO_snprintf(bio_buf, sizeof(bio_buf), data->format,
                                    data->arg1, data->value.i);
        break;

    case 0:
    default:
        bio_ret = test_BIO_snprintf(bio_buf, sizeof(bio_buf), data->format,
                                    data->value.i);
    }

    if (data->skip_libc_check) {
        if (strcmp(bio_buf, data->expected) != 0)
	    TEST_note("%s Result (%s) does not match (%s)", __func__,
                      bio_buf, data->expected);
    } else if (!TEST_str_eq(bio_buf, data->expected)
        + !TEST_int_eq(bio_ret, exp_ret)) {
        TEST_note("Format: \"%s\"", data->format);
        return 0;
    }

    return 1;
}

#ifdef _WIN32
static int test_width_precision_win32(int i)
{
    int ret;

    test_BIO_snprintf = ossl_BIO_snprintf_msvc;
    ret = test_width_precision(i);
    test_BIO_snprintf = BIO_snprintf;

    return ret;
}
#endif

static const struct n_data {
    const char *format;
    const char *expected;
    enum arg_type n_type;
    const uint64_t exp_n;
    enum arg_type arg1_type;
    union ptrint arg1;
    enum arg_type arg2_type;
    union ptrint arg2;
    bool skip_libc_check;
    /* Since OpenSSL's snprintf is non-standard on buffer overflow */
    bool skip_libc_ret_check;
    int exp_ret;
} n_data[] = {
    { "%n", "", AT_INT, 0, AT_NONE },
    { "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz%n",
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
      AT_INT, 62, AT_NONE },
    { "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+=%n",
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+",
      AT_INT, 64, AT_NONE, .skip_libc_ret_check = true, .exp_ret = -1 },
    { "%" PRIdPTR "%hhn", "1234567890",
      AT_CHAR, 10, AT_INT, { .i = 1234567890 } },
    { "%#.200" PRIXPTR "%hhn",
      "0X0000000000000000000000000000000000000000000000000000000000000",
      AT_CHAR, -54, AT_INT, { .i = 1234567890 },
      .skip_libc_ret_check = true, .exp_ret = -1 },
    { "%#10000" PRIoPTR "%hhn1234567890",
      "                                                               ",
      /* XXX Should we overflow or saturate?  glibc does the former. */
      AT_CHAR, 16, AT_INT, { .i = 1234567890 },
      .skip_libc_ret_check = true, .exp_ret = -1 },
    { "%.0s%hn0987654321", "0987654321",
      AT_SHORT, 0, AT_INT, { .s = "1234567890" } },
    { "%-123456s%hn0987654321",
      "1234567890                                                     ",
      AT_SHORT, -7616, AT_INT, { .s = "1234567890" },
      .skip_libc_ret_check = true, .exp_ret = -1 },
#if !defined(OPENSSL_SYS_WINDOWS)
    { "%1234567898.1234567890" PRIxPTR "%n",
      "        0000000000000000000000000000000000000000000000000000000",
      AT_INT, 1234567898, AT_INT, { .i = 0xbadc0ded },
      /* MS CRT can't handle this one, snprintf() causes access violation. */
      .skip_libc_ret_check = true, .exp_ret = -1 },
#endif
    { "%s|%n",
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ|",
      AT_INT, 63, AT_STR, { .s =
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" } },
    { "|%#2147483639x|%2147483639s|0123456789abcdef|%ln",
      "|                                                              ",
      AT_LONG, sizeof(long) == 8 ? 4294967298ULL : 2,
      AT_INT, { .i = 0x1337 }, AT_STR, { .s = "1EE7" },
      /* glibc caps %n value (1 << 32) - 1 */
      .skip_libc_check = true, .exp_ret = -1 },
    { "|%-2147483638s|0123456789abcdef|%02147483637o|0123456789ABCDEF|%lln",
      "|echo test test test                                           ",
      AT_LLONG, 4294967312ULL,
      AT_STR, { .s = "echo test test test" }, AT_INT, { .i = 0xbad },
      /* glibc caps %n value (1 << 32) - 1 */
      .skip_libc_check = true, .exp_ret = -1 },
    { "|%+2147483639s|2147483639|%.2147483639u|2147483639|%zn",
      "|                                                              ",
      AT_SIZE, sizeof(size_t) == 8 ? 4294967303ULL : 7,
      AT_STR, { .s = "according to all known laws of aviation" },
      AT_INT, { .i = 0xbee },
      /* glibc caps %n value (1 << 32) - 1 */
      .skip_libc_check = true, .exp_ret = -1 },
    { "==%2147483639.2147483639s==2147483639.2147483639==%+2147483639d==%tn==",
      "==                                                             ",
      AT_PTRDIFF, sizeof(ptrdiff_t) == 8 ? 4294967307ULL : 11,
      AT_STR, { .s = "oh hi there hello" }, AT_INT, { .i = 0x1234 },
      /* glibc caps %n value (1 << 32) - 1 */
      .skip_libc_check = true, .exp_ret = -1 },
    { "=%2147483639s=%888888888X=%tn=",
      "=                                                              ",
      AT_PTRDIFF, sizeof(ptrdiff_t) == 8 ? 3036372530LL : -1258594766LL,
      AT_STR, { .s = NULL }, AT_INT, { .i = 0xdead },
      .skip_libc_check = true, .exp_ret = -1 },
};

static int test_n(int i)
{
    const struct n_data *data = n_data + i;
    const int exp_ret = data->exp_ret ? data->exp_ret
                                      : (int) strlen(data->expected);
    char bio_buf[64];
    char std_buf[64];
    int bio_ret;
    union {
        uint64_t val;
        signed char hh;
        short h;
        int i;
        long int l;
        long long int ll;
        ossl_ssize_t z;
        ptrdiff_t t;
    } n = { 0 };

#if defined(OPENSSL_SYS_WINDOWS) && !defined(__MINGW32__)
    /*
     * MS CRT is special and throws an exception when %n is used even
     * in non-*_s versions of printf routines, and there is a special function
     * to enable %n handling.
     */
    _set_printf_count_output(1);
    if (_get_printf_count_output() == 0) {
        TEST_note("Can't enable %%n handling for snprintf"
                  ", skipping the checks against libc");
        return 1;
    }
#elif defined(__OpenBSD__)
    {
        static bool note_printed;

        if (!note_printed) {
            TEST_note("OpenBSD libc unconditionally aborts a program "
                      "if %%n is used in a *printf routine"
                      ", skipping the checks against libc");
            note_printed = true;
        }
        return 1;
    }
#endif /* defined(OPENSSL_SYS_WINDOWS) || defined(__OpenBSD__) */

    memset(bio_buf, '@', sizeof(bio_buf));
    memset(std_buf, '#', sizeof(std_buf));

    switch (data->n_type) {
#define DO_PRINT(field_)                                                       \
    do {                                                                       \
        if (data->arg1_type == AT_NONE) {                                      \
            bio_ret = test_BIO_snprintf(bio_buf, sizeof(bio_buf), data->format,\
                                        &n.field_);                            \
        } else if (data->arg2_type == AT_NONE) {                               \
            bio_ret = test_BIO_snprintf(bio_buf, sizeof(bio_buf), data->format,\
                                        data->arg1.i, &n.field_);              \
        } else {                                                               \
            bio_ret = test_BIO_snprintf(bio_buf, sizeof(bio_buf), data->format,\
                                        data->arg1.i, data->arg2.i, &n.field_);\
        }                                                                      \
    } while (0)
    case AT_CHAR:
        DO_PRINT(hh);
        break;
    case AT_SHORT:
        DO_PRINT(h);
        break;
    case AT_INT:
        DO_PRINT(i);
        break;
    case AT_LONG:
        DO_PRINT(l);
        break;
    case AT_LLONG:
        DO_PRINT(ll);
        break;
    case AT_SIZE:
        DO_PRINT(z);
        break;
    case AT_PTRDIFF:
        DO_PRINT(t);
        break;
    default:
        TEST_error("Unexpected arg type: %d", data->n_type);
        return 0;
#undef DO_PRINT
    }

    if (data->skip_libc_check) {
        if (strcmp(bio_buf, data->expected) != 0)
	    TEST_note("%s Result (%s) does not match (%s)", __func__,
                      bio_buf, data->expected);
    } else if (!TEST_str_eq(bio_buf, data->expected)
        + !TEST_int_eq(bio_ret, exp_ret)) {
        TEST_note("Format: \"%s\"", data->format);
        return 0;
    }

    return 1;
}

typedef struct z_data_st {
    size_t value;
    const char *format;
    const char *expected;
} z_data;

static const z_data zu_data[] = {
    { SIZE_MAX, "%zu", (sizeof(size_t) == 4 ? "4294967295"
                        : sizeof(size_t) == 8 ? "18446744073709551615"
                        : "") },
    /*
     * in 2-complement, the unsigned number divided by two plus one becomes the
     * smallest possible negative signed number of the corresponding type
     */
    { SIZE_MAX / 2 + 1, "%zi", (sizeof(size_t) == 4 ? "-2147483648"
                                : sizeof(size_t) == 8 ? "-9223372036854775808"
                                : "") },
    { 0, "%zu", "0" },
    { 0, "%zi", "0" },
};

static int test_zu(int i)
{
    char bio_buf[80];
    const z_data *data = &zu_data[i];
    const int exp_ret = (int) strlen(data->expected);
    int bio_ret;

    memset(bio_buf, '@', sizeof(bio_buf));

    bio_ret = test_BIO_snprintf(bio_buf, sizeof(bio_buf), data->format, data->value);
    if (!TEST_str_eq(bio_buf, data->expected)
        + !TEST_int_eq(bio_ret, exp_ret))
        return 0;

    return 1;
}

#ifdef _WIN32
static int test_zu_win32(int i)
{
    int ret;

    test_BIO_snprintf = ossl_BIO_snprintf_msvc;
    ret = test_zu(i);
    test_BIO_snprintf = BIO_snprintf;

    return ret;
}
#endif

static const struct t_data {
    size_t value;
    const char *format;
    const char *expected;
} t_data[] = {
    { PTRDIFF_MAX, "%+td",
      sizeof(ptrdiff_t) == 4 ? "+2147483647" : "+9223372036854775807" },
    { PTRDIFF_MIN, "%+ti",
      sizeof(ptrdiff_t) == 4 ? "-2147483648" : "-9223372036854775808" },
    { 0, "%tu", "0" },
    { 0, "%+09ti", "+00000000" },
};

static int test_t(int i)
{
    char bio_buf[64];
    const struct t_data *data = &t_data[i];
    const int exp_ret = (int) strlen(data->expected);
    int bio_ret;

    memset(bio_buf, '@', sizeof(bio_buf));

    bio_ret = test_BIO_snprintf(bio_buf, sizeof(bio_buf), data->format, data->value);
    if (!TEST_str_eq(bio_buf, data->expected)
        + !TEST_int_eq(bio_ret, exp_ret))
        return 0;

    return 1;
}

#ifdef _WIN32
static int test_t_win32(int i)
{
    int ret;

    test_BIO_snprintf = ossl_BIO_snprintf_msvc;
    ret = test_t(i);
    test_BIO_snprintf = BIO_snprintf;

    return ret;
}
#endif

typedef struct j_data_st {
    uint64_t value;
    const char *format;
    const char *expected;
} j_data;

static const j_data jf_data[] = {
    { 0xffffffffffffffffULL, "%ju", "18446744073709551615" },
    { 0xffffffffffffffffULL, "%jx", "ffffffffffffffff" },
    { 0x8000000000000000ULL, "%ju", "9223372036854775808" },
    /*
     * These tests imply two's complement, but it's the only binary
     * representation we support, see test/sanitytest.c...
     */
    { 0x8000000000000000ULL, "%ji", "-9223372036854775808" },
};

static int test_j(int i)
{
    const j_data *data = &jf_data[i];
    char bio_buf[80];
    const int exp_ret = (int) strlen(data->expected);
    int bio_ret;

    memset(bio_buf, '@', sizeof(bio_buf));

    bio_ret = test_BIO_snprintf(bio_buf, sizeof(bio_buf), data->format, data->value);
    if (!TEST_str_eq(bio_buf, data->expected)
        + !TEST_int_eq(bio_ret, exp_ret))
        return 0;

    return 1;
}

#ifdef _WIN32
static int test_j_win32(int i)
{
    int ret;

    test_BIO_snprintf = ossl_BIO_snprintf_msvc;
    ret = test_j(i);
    test_BIO_snprintf = BIO_snprintf;

    return ret;
}
#endif

/* Precision and width. */
typedef struct pw_st {
    int p;
    const char *w;
} pw;

static const pw pw_params[] = {
    { 4, "" },
    { 5, "" },
    { 4, "12" },
    { 5, "12" },
    { 0, "" },
    { -1, "" },
    { 4, "08" }
};

static int dofptest(int test, int sub, double val, const char *width, int prec)
{
    static const char *fspecs[] = {
        "e", "f", "g", "E", "G"
    };
    char format[80], result[80];
    int ret = 1, i;
    int exp_ret;
    int bio_ret;

    for (i = 0; i < (int)OSSL_NELEM(fspecs); i++) {
        const char *fspec = fspecs[i];

        memset(result, '@', sizeof(result));

        if (prec >= 0)
            test_BIO_snprintf(format, sizeof(format), "%%%s.%d%s", width, prec,
                              fspec);
        else
            test_BIO_snprintf(format, sizeof(format), "%%%s%s", width, fspec);

        exp_ret = (int) strlen(fpexpected[test][sub][i]);
        bio_ret = test_BIO_snprintf(result, sizeof(result), format, val);

        if (justprint) {
            if (i == 0)
                printf("    /*  %d.%02d */ { \"%s\"", test, sub, result);
            else
                printf(", \"%s\"", result);
        } else {
            if (!TEST_str_eq(fpexpected[test][sub][i], result)
                + !TEST_int_eq(bio_ret, exp_ret)) {
                TEST_info("test %d format=|%s| exp=|%s|, ret=|%s|",
                          test, format, fpexpected[test][sub][i], result);
                ret = 0;
            }
        }
    }
    if (justprint)
        printf(" },\n");
    return ret;
}

static int test_fp(int i)
{
    int t = 0, r;
    const double frac = 2.0 / 3.0;
    const pw *pwp = &pw_params[i];

    if (justprint)
        printf("    {\n");
    r = TEST_true(dofptest(i, t++, 0.0, pwp->w, pwp->p))
        && TEST_true(dofptest(i, t++, 0.67, pwp->w, pwp->p))
        && TEST_true(dofptest(i, t++, frac, pwp->w, pwp->p))
        && TEST_true(dofptest(i, t++, frac / 1000, pwp->w, pwp->p))
        && TEST_true(dofptest(i, t++, frac / 10000, pwp->w, pwp->p))
        && TEST_true(dofptest(i, t++, 6.0 + frac, pwp->w, pwp->p))
        && TEST_true(dofptest(i, t++, 66.0 + frac, pwp->w, pwp->p))
        && TEST_true(dofptest(i, t++, 666.0 + frac, pwp->w, pwp->p))
        && TEST_true(dofptest(i, t++, 6666.0 + frac, pwp->w, pwp->p))
        && TEST_true(dofptest(i, t++, 66666.0 + frac, pwp->w, pwp->p))
        && TEST_true(dofptest(i, t++, -66666.0 - frac, pwp->w, pwp->p));
    if (justprint)
        printf("    },\n");
    return r;
}

#ifdef _WIN32
static int test_fp_win32(int i)
{
    int ret;

    test_BIO_snprintf = ossl_BIO_snprintf_msvc;
    ret = test_fp(i);
    test_BIO_snprintf = BIO_snprintf;

    return ret;
}
#endif

typedef enum OPTION_choice {
    OPT_ERR = -1,
    OPT_EOF = 0,
    OPT_PRINT,
    OPT_TEST_ENUM
} OPTION_CHOICE;

const OPTIONS *test_get_options(void)
{
    static const OPTIONS options[] = {
        OPT_TEST_OPTIONS_DEFAULT_USAGE,
        { "expected", OPT_PRINT, '-', "Output values" },
        { NULL }
    };
    return options;
}

int setup_tests(void)
{
    OPTION_CHOICE o;

    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        case OPT_PRINT:
            justprint = 1;
            break;
        case OPT_TEST_CASES:
            break;
        default:
            return 0;
        }
    }

    ADD_ALL_TESTS(test_fp, OSSL_NELEM(pw_params));
    ADD_ALL_TESTS(test_int, OSSL_NELEM(int_data));
    ADD_ALL_TESTS(test_width_precision, OSSL_NELEM(wp_data));
    ADD_ALL_TESTS(test_n, OSSL_NELEM(n_data));
    ADD_ALL_TESTS(test_zu, OSSL_NELEM(zu_data));
    ADD_ALL_TESTS(test_t, OSSL_NELEM(t_data));
    ADD_ALL_TESTS(test_j, OSSL_NELEM(jf_data));

#ifdef _WIN32
    /*
     * those tests are using _vsnprintf_s()
     */
    ADD_ALL_TESTS(test_fp_win32, OSSL_NELEM(pw_params));
    ADD_ALL_TESTS(test_int_win32, OSSL_NELEM(int_data));
    ADD_ALL_TESTS(test_width_precision_win32, OSSL_NELEM(wp_data));
    /*
     * test_n() which uses "%n" format string triggers
     * an assert 'Incorrect format specifier' found in
     * minkernel\crts\ucrt\correct_internal_stdio_output.h
     * (line 1690).
     * Therefore we don't add test_n() here.
     */
    ADD_ALL_TESTS(test_zu_win32, OSSL_NELEM(zu_data));
    ADD_ALL_TESTS(test_t_win32, OSSL_NELEM(t_data));
    ADD_ALL_TESTS(test_j_win32, OSSL_NELEM(jf_data));
#endif

    return 1;
}

/*
 * Replace testutil output routines.  We do this to eliminate possible sources
 * of BIO error
 */
BIO *bio_out = NULL;
BIO *bio_err = NULL;

static int tap_level = 0;

void test_open_streams(void)
{
}

void test_adjust_streams_tap_level(int level)
{
    tap_level = level;
}

void test_close_streams(void)
{
}

/*
 * This works out as long as caller doesn't use any "fancy" formats.
 * But we are caller's caller, and test_str_eq is the only one called,
 * and it uses only "%s", which is not "fancy"...
 */
int test_vprintf_stdout(const char *fmt, va_list ap)
{
    return fprintf(stdout, "%*s# ", tap_level, "") + vfprintf(stdout, fmt, ap);
}

int test_vprintf_stderr(const char *fmt, va_list ap)
{
    return fprintf(stderr, "%*s# ", tap_level, "") + vfprintf(stderr, fmt, ap);
}

int test_flush_stdout(void)
{
    return fflush(stdout);
}

int test_flush_stderr(void)
{
    return fflush(stderr);
}

int test_vprintf_tapout(const char *fmt, va_list ap)
{
    return fprintf(stdout, "%*s", tap_level, "") + vfprintf(stdout, fmt, ap);
}

int test_vprintf_taperr(const char *fmt, va_list ap)
{
    return fprintf(stderr, "%*s", tap_level, "") + vfprintf(stderr, fmt, ap);
}

int test_flush_tapout(void)
{
    return fflush(stdout);
}

int test_flush_taperr(void)
{
    return fflush(stderr);
}


