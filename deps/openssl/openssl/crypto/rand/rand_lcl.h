/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef HEADER_RAND_LCL_H
# define HEADER_RAND_LCL_H

# define ENTROPY_NEEDED 32      /* require 256 bits = 32 bytes of randomness */

# if !defined(USE_MD5_RAND) && !defined(USE_SHA1_RAND) && !defined(USE_MDC2_RAND) && !defined(USE_MD2_RAND)
#  define USE_SHA1_RAND
# endif

# include <openssl/evp.h>
# define MD_Update(a,b,c)        EVP_DigestUpdate(a,b,c)
# define MD_Final(a,b)           EVP_DigestFinal_ex(a,b,NULL)
# if defined(USE_MD5_RAND)
#  include <openssl/md5.h>
#  define MD_DIGEST_LENGTH        MD5_DIGEST_LENGTH
#  define MD_Init(a)              EVP_DigestInit_ex(a,EVP_md5(), NULL)
#  define MD(a,b,c)               EVP_Digest(a,b,c,NULL,EVP_md5(), NULL)
# elif defined(USE_SHA1_RAND)
#  include <openssl/sha.h>
#  define MD_DIGEST_LENGTH        SHA_DIGEST_LENGTH
#  define MD_Init(a)              EVP_DigestInit_ex(a,EVP_sha1(), NULL)
#  define MD(a,b,c)               EVP_Digest(a,b,c,NULL,EVP_sha1(), NULL)
# elif defined(USE_MDC2_RAND)
#  include <openssl/mdc2.h>
#  define MD_DIGEST_LENGTH        MDC2_DIGEST_LENGTH
#  define MD_Init(a)              EVP_DigestInit_ex(a,EVP_mdc2(), NULL)
#  define MD(a,b,c)               EVP_Digest(a,b,c,NULL,EVP_mdc2(), NULL)
# elif defined(USE_MD2_RAND)
#  include <openssl/md2.h>
#  define MD_DIGEST_LENGTH        MD2_DIGEST_LENGTH
#  define MD_Init(a)              EVP_DigestInit_ex(a,EVP_md2(), NULL)
#  define MD(a,b,c)               EVP_Digest(a,b,c,NULL,EVP_md2(), NULL)
# endif

void rand_hw_xor(unsigned char *buf, size_t num);

#endif
