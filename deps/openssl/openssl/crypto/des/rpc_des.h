/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*  @(#)des.h   2.2 88/08/10 4.0 RPCSRC; from 2.7 88/02/08 SMI  */
/*-
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
/*
 * Generic DES driver interface
 * Keep this file hardware independent!
 * Copyright (c) 1986 by Sun Microsystems, Inc.
 */

#define DES_MAXLEN      65536   /* maximum # of bytes to encrypt */
#define DES_QUICKLEN    16      /* maximum # of bytes to encrypt quickly */

enum desdir { ENCRYPT, DECRYPT };
enum desmode { CBC, ECB };

/*
 * parameters to ioctl call
 */
struct desparams {
    unsigned char des_key[8];   /* key (with low bit parity) */
    enum desdir des_dir;        /* direction */
    enum desmode des_mode;      /* mode */
    unsigned char des_ivec[8];  /* input vector */
    unsigned des_len;           /* number of bytes to crypt */
    union {
        unsigned char UDES_data[DES_QUICKLEN];
        unsigned char *UDES_buf;
    } UDES;
#define des_data UDES.UDES_data /* direct data here if quick */
#define des_buf  UDES.UDES_buf  /* otherwise, pointer to data */
};

/*
 * Encrypt an arbitrary sized buffer
 */
#define DESIOCBLOCK     _IOWR('d', 6, struct desparams)

/*
 * Encrypt of small amount of data, quickly
 */
#define DESIOCQUICK     _IOWR('d', 7, struct desparams)
