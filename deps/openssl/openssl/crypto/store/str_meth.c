/* crypto/store/str_meth.c */
/*
 * Written by Richard Levitte (richard@levitte.org) for the OpenSSL project
 * 2003.
 */
/* ====================================================================
 * Copyright (c) 2003 The OpenSSL Project.  All rights reserved.
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

#include <string.h>
#include <openssl/buffer.h>
#include "str_locl.h"

STORE_METHOD *STORE_create_method(char *name)
{
    STORE_METHOD *store_method =
        (STORE_METHOD *)OPENSSL_malloc(sizeof(STORE_METHOD));

    if (store_method) {
        memset(store_method, 0, sizeof(*store_method));
        store_method->name = BUF_strdup(name);
    }
    return store_method;
}

/*
 * BIG FSCKING WARNING!!!! If you use this on a statically allocated method
 * (that is, it hasn't been allocated using STORE_create_method(), you
 * deserve anything Murphy can throw at you and more! You have been warned.
 */
void STORE_destroy_method(STORE_METHOD *store_method)
{
    if (!store_method)
        return;
    OPENSSL_free(store_method->name);
    store_method->name = NULL;
    OPENSSL_free(store_method);
}

int STORE_method_set_initialise_function(STORE_METHOD *sm,
                                         STORE_INITIALISE_FUNC_PTR init_f)
{
    sm->init = init_f;
    return 1;
}

int STORE_method_set_cleanup_function(STORE_METHOD *sm,
                                      STORE_CLEANUP_FUNC_PTR clean_f)
{
    sm->clean = clean_f;
    return 1;
}

int STORE_method_set_generate_function(STORE_METHOD *sm,
                                       STORE_GENERATE_OBJECT_FUNC_PTR
                                       generate_f)
{
    sm->generate_object = generate_f;
    return 1;
}

int STORE_method_set_get_function(STORE_METHOD *sm,
                                  STORE_GET_OBJECT_FUNC_PTR get_f)
{
    sm->get_object = get_f;
    return 1;
}

int STORE_method_set_store_function(STORE_METHOD *sm,
                                    STORE_STORE_OBJECT_FUNC_PTR store_f)
{
    sm->store_object = store_f;
    return 1;
}

int STORE_method_set_modify_function(STORE_METHOD *sm,
                                     STORE_MODIFY_OBJECT_FUNC_PTR modify_f)
{
    sm->modify_object = modify_f;
    return 1;
}

int STORE_method_set_revoke_function(STORE_METHOD *sm,
                                     STORE_HANDLE_OBJECT_FUNC_PTR revoke_f)
{
    sm->revoke_object = revoke_f;
    return 1;
}

int STORE_method_set_delete_function(STORE_METHOD *sm,
                                     STORE_HANDLE_OBJECT_FUNC_PTR delete_f)
{
    sm->delete_object = delete_f;
    return 1;
}

int STORE_method_set_list_start_function(STORE_METHOD *sm,
                                         STORE_START_OBJECT_FUNC_PTR
                                         list_start_f)
{
    sm->list_object_start = list_start_f;
    return 1;
}

int STORE_method_set_list_next_function(STORE_METHOD *sm,
                                        STORE_NEXT_OBJECT_FUNC_PTR
                                        list_next_f)
{
    sm->list_object_next = list_next_f;
    return 1;
}

int STORE_method_set_list_end_function(STORE_METHOD *sm,
                                       STORE_END_OBJECT_FUNC_PTR list_end_f)
{
    sm->list_object_end = list_end_f;
    return 1;
}

int STORE_method_set_update_store_function(STORE_METHOD *sm,
                                           STORE_GENERIC_FUNC_PTR update_f)
{
    sm->update_store = update_f;
    return 1;
}

int STORE_method_set_lock_store_function(STORE_METHOD *sm,
                                         STORE_GENERIC_FUNC_PTR lock_f)
{
    sm->lock_store = lock_f;
    return 1;
}

int STORE_method_set_unlock_store_function(STORE_METHOD *sm,
                                           STORE_GENERIC_FUNC_PTR unlock_f)
{
    sm->unlock_store = unlock_f;
    return 1;
}

int STORE_method_set_ctrl_function(STORE_METHOD *sm,
                                   STORE_CTRL_FUNC_PTR ctrl_f)
{
    sm->ctrl = ctrl_f;
    return 1;
}

STORE_INITIALISE_FUNC_PTR STORE_method_get_initialise_function(STORE_METHOD
                                                               *sm)
{
    return sm->init;
}

STORE_CLEANUP_FUNC_PTR STORE_method_get_cleanup_function(STORE_METHOD *sm)
{
    return sm->clean;
}

STORE_GENERATE_OBJECT_FUNC_PTR STORE_method_get_generate_function(STORE_METHOD
                                                                  *sm)
{
    return sm->generate_object;
}

STORE_GET_OBJECT_FUNC_PTR STORE_method_get_get_function(STORE_METHOD *sm)
{
    return sm->get_object;
}

STORE_STORE_OBJECT_FUNC_PTR STORE_method_get_store_function(STORE_METHOD *sm)
{
    return sm->store_object;
}

STORE_MODIFY_OBJECT_FUNC_PTR STORE_method_get_modify_function(STORE_METHOD
                                                              *sm)
{
    return sm->modify_object;
}

STORE_HANDLE_OBJECT_FUNC_PTR STORE_method_get_revoke_function(STORE_METHOD
                                                              *sm)
{
    return sm->revoke_object;
}

STORE_HANDLE_OBJECT_FUNC_PTR STORE_method_get_delete_function(STORE_METHOD
                                                              *sm)
{
    return sm->delete_object;
}

STORE_START_OBJECT_FUNC_PTR STORE_method_get_list_start_function(STORE_METHOD
                                                                 *sm)
{
    return sm->list_object_start;
}

STORE_NEXT_OBJECT_FUNC_PTR STORE_method_get_list_next_function(STORE_METHOD
                                                               *sm)
{
    return sm->list_object_next;
}

STORE_END_OBJECT_FUNC_PTR STORE_method_get_list_end_function(STORE_METHOD *sm)
{
    return sm->list_object_end;
}

STORE_GENERIC_FUNC_PTR STORE_method_get_update_store_function(STORE_METHOD
                                                              *sm)
{
    return sm->update_store;
}

STORE_GENERIC_FUNC_PTR STORE_method_get_lock_store_function(STORE_METHOD *sm)
{
    return sm->lock_store;
}

STORE_GENERIC_FUNC_PTR STORE_method_get_unlock_store_function(STORE_METHOD
                                                              *sm)
{
    return sm->unlock_store;
}

STORE_CTRL_FUNC_PTR STORE_method_get_ctrl_function(STORE_METHOD *sm)
{
    return sm->ctrl;
}
