/* crypto/engine/eng_init.c */
/* ====================================================================
 * Copyright (c) 1999-2001 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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

#include "eng_int.h"

/*
 * Initialise a engine type for use (or up its functional reference count if
 * it's already in use). This version is only used internally.
 */
int engine_unlocked_init(ENGINE *e)
{
    int to_return = 1;

    if ((e->funct_ref == 0) && e->init)
        /*
         * This is the first functional reference and the engine requires
         * initialisation so we do it now.
         */
        to_return = e->init(e);
    if (to_return) {
        /*
         * OK, we return a functional reference which is also a structural
         * reference.
         */
        e->struct_ref++;
        e->funct_ref++;
        engine_ref_debug(e, 0, 1)
            engine_ref_debug(e, 1, 1)
    }
    return to_return;
}

/*
 * Free a functional reference to a engine type. This version is only used
 * internally.
 */
int engine_unlocked_finish(ENGINE *e, int unlock_for_handlers)
{
    int to_return = 1;

    /*
     * Reduce the functional reference count here so if it's the terminating
     * case, we can release the lock safely and call the finish() handler
     * without risk of a race. We get a race if we leave the count until
     * after and something else is calling "finish" at the same time -
     * there's a chance that both threads will together take the count from 2
     * to 0 without either calling finish().
     */
    e->funct_ref--;
    engine_ref_debug(e, 1, -1);
    if ((e->funct_ref == 0) && e->finish) {
        if (unlock_for_handlers)
            CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
        to_return = e->finish(e);
        if (unlock_for_handlers)
            CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
        if (!to_return)
            return 0;
    }
#ifdef REF_CHECK
    if (e->funct_ref < 0) {
        fprintf(stderr, "ENGINE_finish, bad functional reference count\n");
        abort();
    }
#endif
    /* Release the structural reference too */
    if (!engine_free_util(e, 0)) {
        ENGINEerr(ENGINE_F_ENGINE_UNLOCKED_FINISH, ENGINE_R_FINISH_FAILED);
        return 0;
    }
    return to_return;
}

/* The API (locked) version of "init" */
int ENGINE_init(ENGINE *e)
{
    int ret;
    if (e == NULL) {
        ENGINEerr(ENGINE_F_ENGINE_INIT, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
    ret = engine_unlocked_init(e);
    CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
    return ret;
}

/* The API (locked) version of "finish" */
int ENGINE_finish(ENGINE *e)
{
    int to_return = 1;

    if (e == NULL) {
        ENGINEerr(ENGINE_F_ENGINE_FINISH, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
    to_return = engine_unlocked_finish(e, 1);
    CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
    if (!to_return) {
        ENGINEerr(ENGINE_F_ENGINE_FINISH, ENGINE_R_FINISH_FAILED);
        return 0;
    }
    return to_return;
}
