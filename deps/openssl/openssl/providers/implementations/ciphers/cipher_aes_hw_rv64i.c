/*
 * Copyright 2022-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*-
 * RISC-V 64 ZKND ZKNE / ZVKNED support for all hardware accelerated AES modes.
 */

#include "internal/deprecated.h"
#include "cipher_aes.h"
#include "cipher_aes_gcm.h"
#include "cipher_aes_ccm.h"
#include "cipher_aes_xts.h"

#if defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 64

static int zvkned_key_schedule_supported(size_t keylen)
{
    if (keylen * 8 == 128 || keylen * 8 == 256) {
        return 1;
    }
    return 0;
}

/* MODES: ecb, cbc, cfb, ofb, ctr */

static int cipher_hw_rv64i_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    if (RISCV_HAS_ZVKNED() && riscv_vlen() >= 128) {
        /*
         * Zvkned only supports 128 and 256 bit keys for key schedule
         * generation. For the AES-192 case, we fallback to the generic
         * `AES_set_encrypt_key`. All Zvkned-based implementations use the
         * same `encrypt-key` scheduling for both encryption and decryption.
         */
        aes_set_encrypt_key_fn fn_set_key = AES_set_encrypt_key;
        aes_block128_f fn_block = NULL;

        if (zvkned_key_schedule_supported(keylen)) {
            fn_set_key = rv64i_zvkned_set_encrypt_key;
        }
        ecb128_f fn_ecb = ctx->enc ? (ecb128_f)rv64i_zvkned_ecb_encrypt : (ecb128_f)rv64i_zvkned_ecb_decrypt;
        cbc128_f fn_cbc = ctx->enc ? (cbc128_f)rv64i_zvkned_cbc_encrypt : (cbc128_f)rv64i_zvkned_cbc_decrypt;
        ctr128_f fn_ctr = RISCV_HAS_ZVKB() ? (ctr128_f)rv64i_zvkb_zvkned_ctr32_encrypt_blocks : NULL;

        /* Zvkned supports aes-128/192/256 encryption and decryption. */
        if ((ctx->mode == EVP_CIPH_ECB_MODE || ctx->mode == EVP_CIPH_CBC_MODE)
            && !ctx->enc) {
            fn_block = rv64i_zvkned_decrypt;
        } else {
            fn_block = rv64i_zvkned_encrypt;
        }
        return ossl_cipher_set_aes_initkey(ctx, key, keylen, fn_set_key,
            fn_block, fn_ecb, fn_cbc, fn_ctr);
    } else if (RISCV_HAS_ZKND_AND_ZKNE()) {
        if ((ctx->mode == EVP_CIPH_ECB_MODE || ctx->mode == EVP_CIPH_CBC_MODE)
            && !ctx->enc) {
            return ossl_cipher_set_aes_initkey(ctx, key, keylen,
                rv64i_zknd_set_decrypt_key, rv64i_zknd_decrypt,
                NULL, NULL, NULL);
        } else {
            return ossl_cipher_set_aes_initkey(ctx, key, keylen,
                rv64i_zkne_set_encrypt_key, rv64i_zkne_encrypt,
                NULL, NULL, NULL);
        }
    }
    return 0;
}

static const PROV_CIPHER_HW rv64i_ecb = {
    cipher_hw_rv64i_initkey,
    ossl_cipher_hw_generic_ecb,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW rv64i_cbc = {
    cipher_hw_rv64i_initkey,
    ossl_cipher_hw_generic_cbc,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW rv64i_cfb128 = {
    cipher_hw_rv64i_initkey,
    ossl_cipher_hw_generic_cfb128,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW rv64i_cfb8 = {
    cipher_hw_rv64i_initkey,
    ossl_cipher_hw_generic_cfb8,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW rv64i_cfb1 = {
    cipher_hw_rv64i_initkey,
    ossl_cipher_hw_generic_cfb1,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW rv64i_ofb128 = {
    cipher_hw_rv64i_initkey,
    ossl_cipher_hw_generic_ofb128,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW rv64i_ctr = {
    cipher_hw_rv64i_initkey,
    ossl_cipher_hw_generic_ctr,
    ossl_cipher_aes_copyctx
};

const PROV_CIPHER_HW *ossl_prov_cipher_hw_rv64i(enum aes_modes mode)
{
    if ((RISCV_HAS_ZVKNED() && riscv_vlen() >= 128)
        || RISCV_HAS_ZKND_AND_ZKNE()) {
        switch (mode) {
        case AES_MODE_ECB:
            return &rv64i_ecb;
        case AES_MODE_CBC:
            return &rv64i_cbc;
        case AES_MODE_CFB128:
            return &rv64i_cfb128;
        case AES_MODE_CFB8:
            return &rv64i_cfb8;
        case AES_MODE_CFB1:
            return &rv64i_cfb1;
        case AES_MODE_OFB128:
            return &rv64i_ofb128;
        case AES_MODE_CTR:
            return &rv64i_ctr;
        default:
            return NULL;
        }
    }
    return NULL;
}

/* MODES: GCM */

/*-
 * RISC-V 64 ZKND and ZKNE support for AES GCM.
 */
static int rv64i_zknd_zkne_gcm_initkey(PROV_GCM_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    return ossl_aes_gcm_hw_initkey(ctx, key, keylen,
        rv64i_zkne_set_encrypt_key, rv64i_zkne_encrypt, NULL);
}

static const PROV_GCM_HW rv64i_zknd_zkne_gcm = {
    rv64i_zknd_zkne_gcm_initkey,
    ossl_gcm_setiv,
    ossl_gcm_aad_update,
    ossl_generic_aes_gcm_cipher_update,
    ossl_gcm_cipher_final,
    ossl_gcm_one_shot
};

/*-
 * RISC-V RV64 ZVKNED support for AES GCM.
 */
static int rv64i_zvkned_gcm_initkey(PROV_GCM_CTX *ctx, const unsigned char *key,
    size_t keylen)
{
    /*
     * Zvkned only supports 128 and 256 bit keys for key schedule generation.
     * For AES-192 case, we could fallback to `AES_set_encrypt_key`.
     */
    if (zvkned_key_schedule_supported(keylen)) {
        return ossl_aes_gcm_hw_initkey(ctx, key, keylen,
            rv64i_zvkned_set_encrypt_key, rv64i_zvkned_encrypt, NULL);
    } else {
        return ossl_aes_gcm_hw_initkey(ctx, key, keylen,
            AES_set_encrypt_key, rv64i_zvkned_encrypt, NULL);
    }
}

static const PROV_GCM_HW rv64i_zvkned_gcm = {
    rv64i_zvkned_gcm_initkey,
    ossl_gcm_setiv,
    ossl_gcm_aad_update,
    ossl_generic_aes_gcm_cipher_update,
    ossl_gcm_cipher_final,
    ossl_gcm_one_shot
};

/*-
 * RISC-V RV64 ZVKB, ZVKG and ZVKNED support for AES GCM.
 */
static int rv64i_zvkb_zvkg_zvkned_gcm_initkey(PROV_GCM_CTX *ctx,
    const unsigned char *key,
    size_t keylen)
{
    /*
     * Zvkned only supports 128 and 256 bit keys for key schedule generation.
     * For AES-192 case, we could fallback to `AES_set_encrypt_key`.
     */
    if (zvkned_key_schedule_supported(keylen)) {
        return ossl_aes_gcm_hw_initkey(ctx, key, keylen,
            rv64i_zvkned_set_encrypt_key, rv64i_zvkned_encrypt,
            rv64i_zvkb_zvkned_ctr32_encrypt_blocks);
    } else {
        return ossl_aes_gcm_hw_initkey(ctx, key, keylen,
            AES_set_encrypt_key, rv64i_zvkned_encrypt,
            rv64i_zvkb_zvkned_ctr32_encrypt_blocks);
    }
}

static const PROV_GCM_HW rv64i_zvkb_zvkg_zvkned_gcm = {
    rv64i_zvkb_zvkg_zvkned_gcm_initkey,
    ossl_gcm_setiv,
    ossl_gcm_aad_update,
    ossl_generic_aes_gcm_cipher_update,
    ossl_gcm_cipher_final,
    ossl_gcm_one_shot
};

const PROV_GCM_HW *ossl_prov_aes_hw_gcm_rv64i(void)
{
    if (RISCV_HAS_ZVKNED() && riscv_vlen() >= 128) {
        if (RISCV_HAS_ZVKB() && RISCV_HAS_ZVKG())
            return &rv64i_zvkb_zvkg_zvkned_gcm;
        return &rv64i_zvkned_gcm;
    }

    if (RISCV_HAS_ZKND_AND_ZKNE()) {
        return &rv64i_zknd_zkne_gcm;
    }

    return NULL;
}

/* MODES: CCM */

static int ccm_rv64i_zknd_zkne_initkey(PROV_CCM_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    return ossl_cipher_set_ccm_aes_initkey(ctx, key, keylen,
        rv64i_zkne_set_encrypt_key, rv64i_zkne_encrypt, NULL, NULL);
}

static const PROV_CCM_HW rv64i_zknd_zkne_ccm = {
    ccm_rv64i_zknd_zkne_initkey,
    ossl_ccm_generic_setiv,
    ossl_ccm_generic_setaad,
    ossl_ccm_generic_auth_encrypt,
    ossl_ccm_generic_auth_decrypt,
    ossl_ccm_generic_gettag
};

/*-
 * RISC-V RV64 ZVKNED support for AES CCM.
 * This file is included by cipher_aes_ccm_hw.c
 */

static int ccm_rv64i_zvkned_initkey(PROV_CCM_CTX *ctx, const unsigned char *key,
    size_t keylen)
{
    /* Zvkned only supports 128 and 256 bit keys for key schedule generation. */
    if (zvkned_key_schedule_supported(keylen)) {
        return ossl_cipher_set_ccm_aes_initkey(ctx, key, keylen,
            rv64i_zvkned_set_encrypt_key, rv64i_zvkned_encrypt, NULL, NULL);
    } else {
        return ossl_cipher_set_ccm_aes_initkey(ctx, key, keylen,
            AES_set_encrypt_key, rv64i_zvkned_encrypt, NULL, NULL);
    }
}

static const PROV_CCM_HW rv64i_zvkned_ccm = {
    ccm_rv64i_zvkned_initkey,
    ossl_ccm_generic_setiv,
    ossl_ccm_generic_setaad,
    ossl_ccm_generic_auth_encrypt,
    ossl_ccm_generic_auth_decrypt,
    ossl_ccm_generic_gettag
};

const PROV_CCM_HW *ossl_prov_aes_hw_ccm_rv64i(void)
{
    if (RISCV_HAS_ZVKNED() && riscv_vlen() >= 128)
        return &rv64i_zvkned_ccm;
    else if (RISCV_HAS_ZKND_AND_ZKNE())
        return &rv64i_zknd_zkne_ccm;
    else
        return NULL;
}

/* MODES: XTS */

static int cipher_hw_aes_xts_rv64i_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    if (RISCV_HAS_ZVBB() && RISCV_HAS_ZVKG() && RISCV_HAS_ZVKNED() && riscv_vlen() >= 128) {
        /* Zvkned only supports 128 and 256 bit keys. */
        if (zvkned_key_schedule_supported(keylen / 2)) {
            return ossl_cipher_set_aes_xts_initkey(ctx, key, keylen,
                rv64i_zvkned_set_encrypt_key, rv64i_zvkned_set_decrypt_key,
                rv64i_zvkned_encrypt, rv64i_zvkned_decrypt,
                rv64i_zvbb_zvkg_zvkned_aes_xts_encrypt,
                rv64i_zvbb_zvkg_zvkned_aes_xts_decrypt);
        }
        return ossl_cipher_set_aes_xts_initkey(ctx, key, keylen,
            AES_set_encrypt_key, AES_set_encrypt_key,
            rv64i_zvkned_encrypt, rv64i_zvkned_decrypt, NULL, NULL);
    }

    if (RISCV_HAS_ZVKNED() && riscv_vlen() >= 128) {
        /* Zvkned only supports 128 and 256 bit keys. */
        if (zvkned_key_schedule_supported(keylen / 2)) {
            return ossl_cipher_set_aes_xts_initkey(ctx, key, keylen,
                rv64i_zvkned_set_encrypt_key, rv64i_zvkned_set_decrypt_key,
                rv64i_zvkned_encrypt, rv64i_zvkned_decrypt, NULL, NULL);
        }
        return ossl_cipher_set_aes_xts_initkey(ctx, key, keylen,
            AES_set_encrypt_key, AES_set_encrypt_key,
            rv64i_zvkned_encrypt, rv64i_zvkned_decrypt, NULL, NULL);
    }

    if (RISCV_HAS_ZKND_AND_ZKNE()) {
        return ossl_cipher_set_aes_xts_initkey(ctx, key, keylen,
            rv64i_zkne_set_encrypt_key, rv64i_zknd_set_decrypt_key,
            rv64i_zkne_encrypt, rv64i_zknd_decrypt, NULL, NULL);
    }

    return 0;
}

static const PROV_CIPHER_HW aes_xts_rv64i = {
    cipher_hw_aes_xts_rv64i_initkey,
    NULL,
    ossl_cipher_hw_aes_xts_copyctx
};

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_xts_rv64i(void)
{
    if ((RISCV_HAS_ZVKNED() && riscv_vlen() >= 128)
        || RISCV_HAS_ZKND_AND_ZKNE())
        return &aes_xts_rv64i;
    return NULL;
}

#endif
