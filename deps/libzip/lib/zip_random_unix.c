/*
  zip_random_unix.c -- fill the user's buffer with random stuff (Unix version)
  Copyright (C) 2016-2021 Dieter Baron and Thomas Klausner

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

#include "zipint.h"

#ifdef HAVE_CRYPTO
#include "zip_crypto.h"
#endif

#ifdef HAVE_ARC4RANDOM

#include <stdlib.h>

#ifndef HAVE_SECURE_RANDOM
ZIP_EXTERN bool
zip_secure_random(zip_uint8_t *buffer, zip_uint16_t length) {
    arc4random_buf(buffer, length);
    return true;
}
#endif

#ifndef HAVE_RANDOM_UINT32
zip_uint32_t
zip_random_uint32(void) {
    return arc4random();
}
#endif

#else /* HAVE_ARC4RANDOM */

#ifndef HAVE_SECURE_RANDOM
#include <fcntl.h>
#include <unistd.h>

ZIP_EXTERN bool
zip_secure_random(zip_uint8_t *buffer, zip_uint16_t length) {
    int fd;

    if ((fd = open("/dev/urandom", O_RDONLY)) < 0) {
        return false;
    }

    if (read(fd, buffer, length) != length) {
        close(fd);
        return false;
    }

    close(fd);
    return true;
}
#endif

#ifndef HAVE_RANDOM_UINT32
#include <stdlib.h>

zip_uint32_t
zip_random_uint32(void) {
    static bool seeded = false;

    zip_uint32_t value;

    if (zip_secure_random((zip_uint8_t *)&value, sizeof(value))) {
        return value;
    }

    if (!seeded) {
        srandom((unsigned int)time(NULL));
    }

    return (zip_uint32_t)random();
}
#endif

#endif /* HAVE_ARC4RANDOM */
