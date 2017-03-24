/*
 * Copyright 2001-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/crypto.h>
#include <openssl/opensslconf.h>

#if defined(__OpenBSD__) || (defined(__FreeBSD__) && __FreeBSD__ > 2)

# include OPENSSL_UNISTD

int OPENSSL_issetugid(void)
{
    return issetugid();
}

#elif defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_VXWORKS)

int OPENSSL_issetugid(void)
{
    return 0;
}

#else

# include OPENSSL_UNISTD
# include <sys/types.h>

int OPENSSL_issetugid(void)
{
    if (getuid() != geteuid())
        return 1;
    if (getgid() != getegid())
        return 1;
    return 0;
}
#endif
