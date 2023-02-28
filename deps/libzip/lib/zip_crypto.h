/*
  zip_crypto.h -- crypto definitions
  Copyright (C) 2017-2021 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <info@libzip.org>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in
  the documentation and/or other materials provided with the
  distribution.
  3. The names of the authors may not be used to endorse or promote
  products derived from this software without specific prior
  written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef HAD_ZIP_CRYPTO_H
#define HAD_ZIP_CRYPTO_H

#define ZIP_CRYPTO_SHA1_LENGTH 20
#define ZIP_CRYPTO_AES_BLOCK_LENGTH 16

#if defined(HAVE_WINDOWS_CRYPTO)
#include "zip_crypto_win.h"
#elif defined(HAVE_COMMONCRYPTO)
#include "zip_crypto_commoncrypto.h"
#elif defined(HAVE_GNUTLS)
#include "zip_crypto_gnutls.h"
#elif defined(HAVE_OPENSSL)
#include "zip_crypto_openssl.h"
#elif defined(HAVE_MBEDTLS)
#include "zip_crypto_mbedtls.h"
#else
#error "no crypto backend found"
#endif

#endif /*  HAD_ZIP_CRYPTO_H */
