/* crypto/rand/rand_vms.c */
/*
 * Written by Richard Levitte <richard@levitte.org> for the OpenSSL project
 * 2000.
 */
/*
 * Modified by VMS Software, Inc (2016)
 *    Eliminate looping through all processes (performance)
 *    Add additional randomizations using rand() function
 */
/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <openssl/rand.h>
#include "rand_lcl.h"

#if defined(OPENSSL_SYS_VMS)
# include <descrip.h>
# include <jpidef.h>
# include <ssdef.h>
# include <starlet.h>
# include <efndef>
# ifdef __DECC
#  pragma message disable DOLLARID
# endif

/*
 * Use 32-bit pointers almost everywhere.  Define the type to which to cast a
 * pointer passed to an external function.
 */
# if __INITIAL_POINTER_SIZE == 64
#  define PTR_T __void_ptr64
#  pragma pointer_size save
#  pragma pointer_size 32
# else                          /* __INITIAL_POINTER_SIZE == 64 */
#  define PTR_T void *
# endif                         /* __INITIAL_POINTER_SIZE == 64 [else] */

static struct items_data_st {
    short length, code;         /* length is number of bytes */
} items_data[] = {
    {4, JPI$_BUFIO},
    {4, JPI$_CPUTIM},
    {4, JPI$_DIRIO},
    {4, JPI$_IMAGECOUNT},
    {8, JPI$_LAST_LOGIN_I},
    {8, JPI$_LOGINTIM},
    {4, JPI$_PAGEFLTS},
    {4, JPI$_PID},
    {4, JPI$_PPGCNT},
    {4, JPI$_WSPEAK},
    {4, JPI$_FINALEXC},
    {0, 0}                      /* zero terminated */
};

int RAND_poll(void)
{

    /* determine the number of items in the JPI array */

    struct items_data_st item_entry;
    int item_entry_count = sizeof(items_data)/sizeof(item_entry);

    /* Create the JPI itemlist array to hold item_data content */

    struct {
        short length, code;
        int *buffer;
        int *retlen;
    } item[item_entry_count], *pitem; /* number of entries in items_data */

    struct items_data_st *pitems_data;
    pitems_data = items_data;
    pitem = item;
    int data_buffer[(item_entry_count*2)+4]; /* 8 bytes per entry max */
    int iosb[2];
    int sys_time[2];
    int *ptr;
    int i, j ;
    int tmp_length   = 0;
    int total_length = 0;

    /* Setup itemlist for GETJPI */

    while (pitems_data->length) {
        pitem->length = pitems_data->length;
        pitem->code   = pitems_data->code;
        pitem->buffer = &data_buffer[total_length];
        pitem->retlen = 0;
        /* total_length is in longwords */
        total_length += pitems_data->length/4;
        pitems_data++;
        pitem ++;
    }
    pitem->length = pitem->code = 0;

    /* Fill data_buffer with various info bits from this process */
    /* and twist that data to seed the SSL random number init    */

    if (sys$getjpiw(EFN$C_ENF, NULL, NULL, item, &iosb, 0, 0) == SS$_NORMAL) {
        for (i = 0; i < total_length; i++) {
            sys$gettim((struct _generic_64 *)&sys_time[0]);
            srand(sys_time[0] * data_buffer[0] * data_buffer[1] + i);

            if (i == (total_length - 1)) { /* for JPI$_FINALEXC */
                ptr = &data_buffer[i];
                for (j = 0; j < 4; j++) {
                    data_buffer[i + j] = ptr[j];
                    /* OK to use rand() just to scramble the seed */
                    data_buffer[i + j] ^= (sys_time[0] ^ rand());
                    tmp_length++;
                }
            } else {
                /* OK to use rand() just to scramble the seed */
                data_buffer[i] ^= (sys_time[0] ^ rand());
            }
        }

        total_length += (tmp_length - 1);

        /* size of seed is total_length*4 bytes (64bytes) */
        RAND_add((PTR_T) data_buffer, total_length*4, total_length * 2);
    } else {
        return 0;
    }

    return 1;
}
#endif
