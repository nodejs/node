/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_PROVIDERS_SKEYMGMT_LCL_H
#define OSSL_PROVIDERS_SKEYMGMT_LCL_H
#pragma once
#include <openssl/core_dispatch.h>

OSSL_FUNC_skeymgmt_import_fn generic_import;
OSSL_FUNC_skeymgmt_export_fn generic_export;
OSSL_FUNC_skeymgmt_free_fn generic_free;
OSSL_FUNC_skeymgmt_imp_settable_params_fn generic_imp_settable_params;
OSSL_FUNC_skeymgmt_get_key_id_fn generic_get_key_id;
OSSL_FUNC_skeymgmt_get_local_keyid_fn generic_get_local_keyid;
OSSL_FUNC_skeymgmt_get_algorithm_id_fn generic_get_algorithm_id;

#endif
