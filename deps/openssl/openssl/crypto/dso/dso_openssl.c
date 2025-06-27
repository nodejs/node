/*
 * Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "dso_local.h"

#ifdef DSO_NONE

static DSO_METHOD dso_meth_null = {
    "NULL shared library method"
};

DSO_METHOD *DSO_METHOD_openssl(void)
{
    return &dso_meth_null;
}
#endif
