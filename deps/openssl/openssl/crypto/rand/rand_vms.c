/* crypto/rand/rand_vms.c */
/*
 * Written by Richard Levitte <richard@levitte.org> for the OpenSSL project
 * 2000.
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
    short length, code;         /* length is amount of bytes */
} items_data[] = {
    {
        4, JPI$_BUFIO
    },
    {
        4, JPI$_CPUTIM
    },
    {
        4, JPI$_DIRIO
    },
    {
        8, JPI$_LOGINTIM
    },
    {
        4, JPI$_PAGEFLTS
    },
    {
        4, JPI$_PID
    },
    {
        4, JPI$_WSSIZE
    },
    {
        0, 0
    }
};

int RAND_poll(void)
{
    long pid, iosb[2];
    int status = 0;
    struct {
        short length, code;
        long *buffer;
        int *retlen;
    } item[32], *pitem;
    unsigned char data_buffer[256];
    short total_length = 0;
    struct items_data_st *pitems_data;

    pitems_data = items_data;
    pitem = item;

    /* Setup */
    while (pitems_data->length && (total_length + pitems_data->length <= 256)) {
        pitem->length = pitems_data->length;
        pitem->code = pitems_data->code;
        pitem->buffer = (long *)&data_buffer[total_length];
        pitem->retlen = 0;
        total_length += pitems_data->length;
        pitems_data++;
        pitem ++;
    }
    pitem->length = pitem->code = 0;

    /*
     * Scan through all the processes in the system and add entropy with
     * results from the processes that were possible to look at.
     * However, view the information as only half trustable.
     */
    pid = -1;                   /* search context */
    while ((status = sys$getjpiw(0, &pid, 0, item, iosb, 0, 0))
           != SS$_NOMOREPROC) {
        if (status == SS$_NORMAL) {
            RAND_add((PTR_T) data_buffer, total_length, total_length / 2);
        }
    }
    sys$gettim(iosb);
    RAND_add((PTR_T) iosb, sizeof(iosb), sizeof(iosb) / 2);
    return 1;
}

#endif
