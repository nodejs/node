/* crypto/ex_data.c */

/*
 * Overhaul notes;
 *
 * This code is now *mostly* thread-safe. It is now easier to understand in what
 * ways it is safe and in what ways it is not, which is an improvement. Firstly,
 * all per-class stacks and index-counters for ex_data are stored in the same
 * global LHASH table (keyed by class). This hash table uses locking for all
 * access with the exception of CRYPTO_cleanup_all_ex_data(), which must only be
 * called when no other threads can possibly race against it (even if it was
 * locked, the race would mean it's possible the hash table might have been
 * recreated after the cleanup). As classes can only be added to the hash table,
 * and within each class, the stack of methods can only be incremented, the
 * locking mechanics are simpler than they would otherwise be. For example, the
 * new/dup/free ex_data functions will lock the hash table, copy the method
 * pointers it needs from the relevant class, then unlock the hash table before
 * actually applying those method pointers to the task of the new/dup/free
 * operations. As they can't be removed from the method-stack, only
 * supplemented, there's no race conditions associated with using them outside
 * the lock. The get/set_ex_data functions are not locked because they do not
 * involve this global state at all - they operate directly with a previously
 * obtained per-class method index and a particular "ex_data" variable. These
 * variables are usually instantiated per-context (eg. each RSA structure has
 * one) so locking on read/write access to that variable can be locked locally
 * if required (eg. using the "RSA" lock to synchronise access to a
 * per-RSA-structure ex_data variable if required).
 * [Geoff]
 */

/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
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

#include "cryptlib.h"
#include <openssl/lhash.h>

/* What an "implementation of ex_data functionality" looks like */
struct st_CRYPTO_EX_DATA_IMPL {
        /*********************/
    /* GLOBAL OPERATIONS */
    /* Return a new class index */
    int (*cb_new_class) (void);
    /* Cleanup all state used by the implementation */
    void (*cb_cleanup) (void);
        /************************/
    /* PER-CLASS OPERATIONS */
    /* Get a new method index within a class */
    int (*cb_get_new_index) (int class_index, long argl, void *argp,
                             CRYPTO_EX_new *new_func, CRYPTO_EX_dup *dup_func,
                             CRYPTO_EX_free *free_func);
    /* Initialise a new CRYPTO_EX_DATA of a given class */
    int (*cb_new_ex_data) (int class_index, void *obj, CRYPTO_EX_DATA *ad);
    /* Duplicate a CRYPTO_EX_DATA of a given class onto a copy */
    int (*cb_dup_ex_data) (int class_index, CRYPTO_EX_DATA *to,
                           CRYPTO_EX_DATA *from);
    /* Cleanup a CRYPTO_EX_DATA of a given class */
    void (*cb_free_ex_data) (int class_index, void *obj, CRYPTO_EX_DATA *ad);
};

/* The implementation we use at run-time */
static const CRYPTO_EX_DATA_IMPL *impl = NULL;

/*
 * To call "impl" functions, use this macro rather than referring to 'impl'
 * directly, eg. EX_IMPL(get_new_index)(...);
 */
#define EX_IMPL(a) impl->cb_##a

/* Predeclare the "default" ex_data implementation */
static int int_new_class(void);
static void int_cleanup(void);
static int int_get_new_index(int class_index, long argl, void *argp,
                             CRYPTO_EX_new *new_func, CRYPTO_EX_dup *dup_func,
                             CRYPTO_EX_free *free_func);
static int int_new_ex_data(int class_index, void *obj, CRYPTO_EX_DATA *ad);
static int int_dup_ex_data(int class_index, CRYPTO_EX_DATA *to,
                           CRYPTO_EX_DATA *from);
static void int_free_ex_data(int class_index, void *obj, CRYPTO_EX_DATA *ad);
static CRYPTO_EX_DATA_IMPL impl_default = {
    int_new_class,
    int_cleanup,
    int_get_new_index,
    int_new_ex_data,
    int_dup_ex_data,
    int_free_ex_data
};

/*
 * Internal function that checks whether "impl" is set and if not, sets it to
 * the default.
 */
static void impl_check(void)
{
    CRYPTO_w_lock(CRYPTO_LOCK_EX_DATA);
    if (!impl)
        impl = &impl_default;
    CRYPTO_w_unlock(CRYPTO_LOCK_EX_DATA);
}

/*
 * A macro wrapper for impl_check that first uses a non-locked test before
 * invoking the function (which checks again inside a lock).
 */
#define IMPL_CHECK if(!impl) impl_check();

/* API functions to get/set the "ex_data" implementation */
const CRYPTO_EX_DATA_IMPL *CRYPTO_get_ex_data_implementation(void)
{
    IMPL_CHECK return impl;
}

int CRYPTO_set_ex_data_implementation(const CRYPTO_EX_DATA_IMPL *i)
{
    int toret = 0;
    CRYPTO_w_lock(CRYPTO_LOCK_EX_DATA);
    if (!impl) {
        impl = i;
        toret = 1;
    }
    CRYPTO_w_unlock(CRYPTO_LOCK_EX_DATA);
    return toret;
}

/****************************************************************************/
/*
 * Interal (default) implementation of "ex_data" support. API functions are
 * further down.
 */

/*
 * The type that represents what each "class" used to implement locally. A
 * STACK of CRYPTO_EX_DATA_FUNCS plus a index-counter. The 'class_index' is
 * the global value representing the class that is used to distinguish these
 * items.
 */
typedef struct st_ex_class_item {
    int class_index;
    STACK_OF(CRYPTO_EX_DATA_FUNCS) *meth;
    int meth_num;
} EX_CLASS_ITEM;

/* When assigning new class indexes, this is our counter */
static int ex_class = CRYPTO_EX_INDEX_USER;

/* The global hash table of EX_CLASS_ITEM items */
DECLARE_LHASH_OF(EX_CLASS_ITEM);
static LHASH_OF(EX_CLASS_ITEM) *ex_data = NULL;

/* The callbacks required in the "ex_data" hash table */
static unsigned long ex_class_item_hash(const EX_CLASS_ITEM *a)
{
    return a->class_index;
}

static IMPLEMENT_LHASH_HASH_FN(ex_class_item, EX_CLASS_ITEM)

static int ex_class_item_cmp(const EX_CLASS_ITEM *a, const EX_CLASS_ITEM *b)
{
    return a->class_index - b->class_index;
}

static IMPLEMENT_LHASH_COMP_FN(ex_class_item, EX_CLASS_ITEM)

/*
 * Internal functions used by the "impl_default" implementation to access the
 * state
 */
static int ex_data_check(void)
{
    int toret = 1;
    CRYPTO_w_lock(CRYPTO_LOCK_EX_DATA);
    if (!ex_data && (ex_data = lh_EX_CLASS_ITEM_new()) == NULL)
        toret = 0;
    CRYPTO_w_unlock(CRYPTO_LOCK_EX_DATA);
    return toret;
}

/*
 * This macros helps reduce the locking from repeated checks because the
 * ex_data_check() function checks ex_data again inside a lock.
 */
#define EX_DATA_CHECK(iffail) if(!ex_data && !ex_data_check()) {iffail}

/* This "inner" callback is used by the callback function that follows it */
static void def_cleanup_util_cb(CRYPTO_EX_DATA_FUNCS *funcs)
{
    OPENSSL_free(funcs);
}

/*
 * This callback is used in lh_doall to destroy all EX_CLASS_ITEM values from
 * "ex_data" prior to the ex_data hash table being itself destroyed. Doesn't
 * do any locking.
 */
static void def_cleanup_cb(void *a_void)
{
    EX_CLASS_ITEM *item = (EX_CLASS_ITEM *)a_void;
    sk_CRYPTO_EX_DATA_FUNCS_pop_free(item->meth, def_cleanup_util_cb);
    OPENSSL_free(item);
}

/*
 * Return the EX_CLASS_ITEM from the "ex_data" hash table that corresponds to
 * a given class. Handles locking.
 */
static EX_CLASS_ITEM *def_get_class(int class_index)
{
    EX_CLASS_ITEM d, *p, *gen;
    EX_DATA_CHECK(return NULL;)
        d.class_index = class_index;
    CRYPTO_w_lock(CRYPTO_LOCK_EX_DATA);
    p = lh_EX_CLASS_ITEM_retrieve(ex_data, &d);
    if (!p) {
        gen = OPENSSL_malloc(sizeof(EX_CLASS_ITEM));
        if (gen) {
            gen->class_index = class_index;
            gen->meth_num = 0;
            gen->meth = sk_CRYPTO_EX_DATA_FUNCS_new_null();
            if (!gen->meth)
                OPENSSL_free(gen);
            else {
                /*
                 * Because we're inside the ex_data lock, the return value
                 * from the insert will be NULL
                 */
                (void)lh_EX_CLASS_ITEM_insert(ex_data, gen);
                p = gen;
            }
        }
    }
    CRYPTO_w_unlock(CRYPTO_LOCK_EX_DATA);
    if (!p)
        CRYPTOerr(CRYPTO_F_DEF_GET_CLASS, ERR_R_MALLOC_FAILURE);
    return p;
}

/*
 * Add a new method to the given EX_CLASS_ITEM and return the corresponding
 * index (or -1 for error). Handles locking.
 */
static int def_add_index(EX_CLASS_ITEM *item, long argl, void *argp,
                         CRYPTO_EX_new *new_func, CRYPTO_EX_dup *dup_func,
                         CRYPTO_EX_free *free_func)
{
    int toret = -1;
    CRYPTO_EX_DATA_FUNCS *a =
        (CRYPTO_EX_DATA_FUNCS *)OPENSSL_malloc(sizeof(CRYPTO_EX_DATA_FUNCS));
    if (!a) {
        CRYPTOerr(CRYPTO_F_DEF_ADD_INDEX, ERR_R_MALLOC_FAILURE);
        return -1;
    }
    a->argl = argl;
    a->argp = argp;
    a->new_func = new_func;
    a->dup_func = dup_func;
    a->free_func = free_func;
    CRYPTO_w_lock(CRYPTO_LOCK_EX_DATA);
    while (sk_CRYPTO_EX_DATA_FUNCS_num(item->meth) <= item->meth_num) {
        if (!sk_CRYPTO_EX_DATA_FUNCS_push(item->meth, NULL)) {
            CRYPTOerr(CRYPTO_F_DEF_ADD_INDEX, ERR_R_MALLOC_FAILURE);
            OPENSSL_free(a);
            goto err;
        }
    }
    toret = item->meth_num++;
    (void)sk_CRYPTO_EX_DATA_FUNCS_set(item->meth, toret, a);
 err:
    CRYPTO_w_unlock(CRYPTO_LOCK_EX_DATA);
    return toret;
}

/**************************************************************/
/* The functions in the default CRYPTO_EX_DATA_IMPL structure */

static int int_new_class(void)
{
    int toret;
    CRYPTO_w_lock(CRYPTO_LOCK_EX_DATA);
    toret = ex_class++;
    CRYPTO_w_unlock(CRYPTO_LOCK_EX_DATA);
    return toret;
}

static void int_cleanup(void)
{
    EX_DATA_CHECK(return;)
        lh_EX_CLASS_ITEM_doall(ex_data, def_cleanup_cb);
    lh_EX_CLASS_ITEM_free(ex_data);
    ex_data = NULL;
    impl = NULL;
}

static int int_get_new_index(int class_index, long argl, void *argp,
                             CRYPTO_EX_new *new_func, CRYPTO_EX_dup *dup_func,
                             CRYPTO_EX_free *free_func)
{
    EX_CLASS_ITEM *item = def_get_class(class_index);
    if (!item)
        return -1;
    return def_add_index(item, argl, argp, new_func, dup_func, free_func);
}

/*
 * Thread-safe by copying a class's array of "CRYPTO_EX_DATA_FUNCS" entries
 * in the lock, then using them outside the lock. NB: Thread-safety only
 * applies to the global "ex_data" state (ie. class definitions), not
 * thread-safe on 'ad' itself.
 */
static int int_new_ex_data(int class_index, void *obj, CRYPTO_EX_DATA *ad)
{
    int mx, i;
    void *ptr;
    CRYPTO_EX_DATA_FUNCS **storage = NULL;
    EX_CLASS_ITEM *item = def_get_class(class_index);
    if (!item)
        /* error is already set */
        return 0;
    ad->sk = NULL;
    CRYPTO_r_lock(CRYPTO_LOCK_EX_DATA);
    mx = sk_CRYPTO_EX_DATA_FUNCS_num(item->meth);
    if (mx > 0) {
        storage = OPENSSL_malloc(mx * sizeof(CRYPTO_EX_DATA_FUNCS *));
        if (!storage)
            goto skip;
        for (i = 0; i < mx; i++)
            storage[i] = sk_CRYPTO_EX_DATA_FUNCS_value(item->meth, i);
    }
 skip:
    CRYPTO_r_unlock(CRYPTO_LOCK_EX_DATA);
    if ((mx > 0) && !storage) {
        CRYPTOerr(CRYPTO_F_INT_NEW_EX_DATA, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    for (i = 0; i < mx; i++) {
        if (storage[i] && storage[i]->new_func) {
            ptr = CRYPTO_get_ex_data(ad, i);
            storage[i]->new_func(obj, ptr, ad, i,
                                 storage[i]->argl, storage[i]->argp);
        }
    }
    if (storage)
        OPENSSL_free(storage);
    return 1;
}

/* Same thread-safety notes as for "int_new_ex_data" */
static int int_dup_ex_data(int class_index, CRYPTO_EX_DATA *to,
                           CRYPTO_EX_DATA *from)
{
    int mx, j, i;
    char *ptr;
    CRYPTO_EX_DATA_FUNCS **storage = NULL;
    EX_CLASS_ITEM *item;
    if (!from->sk)
        /* 'to' should be "blank" which *is* just like 'from' */
        return 1;
    if ((item = def_get_class(class_index)) == NULL)
        return 0;
    CRYPTO_r_lock(CRYPTO_LOCK_EX_DATA);
    mx = sk_CRYPTO_EX_DATA_FUNCS_num(item->meth);
    j = sk_void_num(from->sk);
    if (j < mx)
        mx = j;
    if (mx > 0) {
        storage = OPENSSL_malloc(mx * sizeof(CRYPTO_EX_DATA_FUNCS *));
        if (!storage)
            goto skip;
        for (i = 0; i < mx; i++)
            storage[i] = sk_CRYPTO_EX_DATA_FUNCS_value(item->meth, i);
    }
 skip:
    CRYPTO_r_unlock(CRYPTO_LOCK_EX_DATA);
    if ((mx > 0) && !storage) {
        CRYPTOerr(CRYPTO_F_INT_DUP_EX_DATA, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    for (i = 0; i < mx; i++) {
        ptr = CRYPTO_get_ex_data(from, i);
        if (storage[i] && storage[i]->dup_func)
            storage[i]->dup_func(to, from, &ptr, i,
                                 storage[i]->argl, storage[i]->argp);
        CRYPTO_set_ex_data(to, i, ptr);
    }
    if (storage)
        OPENSSL_free(storage);
    return 1;
}

/* Same thread-safety notes as for "int_new_ex_data" */
static void int_free_ex_data(int class_index, void *obj, CRYPTO_EX_DATA *ad)
{
    int mx, i;
    EX_CLASS_ITEM *item;
    void *ptr;
    CRYPTO_EX_DATA_FUNCS **storage = NULL;
    if (ex_data == NULL)
        return;
    if ((item = def_get_class(class_index)) == NULL)
        return;
    CRYPTO_r_lock(CRYPTO_LOCK_EX_DATA);
    mx = sk_CRYPTO_EX_DATA_FUNCS_num(item->meth);
    if (mx > 0) {
        storage = OPENSSL_malloc(mx * sizeof(CRYPTO_EX_DATA_FUNCS *));
        if (!storage)
            goto skip;
        for (i = 0; i < mx; i++)
            storage[i] = sk_CRYPTO_EX_DATA_FUNCS_value(item->meth, i);
    }
 skip:
    CRYPTO_r_unlock(CRYPTO_LOCK_EX_DATA);
    if ((mx > 0) && !storage) {
        CRYPTOerr(CRYPTO_F_INT_FREE_EX_DATA, ERR_R_MALLOC_FAILURE);
        return;
    }
    for (i = 0; i < mx; i++) {
        if (storage[i] && storage[i]->free_func) {
            ptr = CRYPTO_get_ex_data(ad, i);
            storage[i]->free_func(obj, ptr, ad, i,
                                  storage[i]->argl, storage[i]->argp);
        }
    }
    if (storage)
        OPENSSL_free(storage);
    if (ad->sk) {
        sk_void_free(ad->sk);
        ad->sk = NULL;
    }
}

/********************************************************************/
/*
 * API functions that defer all "state" operations to the "ex_data"
 * implementation we have set.
 */

/*
 * Obtain an index for a new class (not the same as getting a new index
 * within an existing class - this is actually getting a new *class*)
 */
int CRYPTO_ex_data_new_class(void)
{
    IMPL_CHECK return EX_IMPL(new_class) ();
}

/*
 * Release all "ex_data" state to prevent memory leaks. This can't be made
 * thread-safe without overhauling a lot of stuff, and shouldn't really be
 * called under potential race-conditions anyway (it's for program shutdown
 * after all).
 */
void CRYPTO_cleanup_all_ex_data(void)
{
    IMPL_CHECK EX_IMPL(cleanup) ();
}

/* Inside an existing class, get/register a new index. */
int CRYPTO_get_ex_new_index(int class_index, long argl, void *argp,
                            CRYPTO_EX_new *new_func, CRYPTO_EX_dup *dup_func,
                            CRYPTO_EX_free *free_func)
{
    int ret = -1;

    IMPL_CHECK
        ret = EX_IMPL(get_new_index) (class_index,
                                      argl, argp, new_func, dup_func,
                                      free_func);
    return ret;
}

/*
 * Initialise a new CRYPTO_EX_DATA for use in a particular class - including
 * calling new() callbacks for each index in the class used by this variable
 */
int CRYPTO_new_ex_data(int class_index, void *obj, CRYPTO_EX_DATA *ad)
{
    IMPL_CHECK return EX_IMPL(new_ex_data) (class_index, obj, ad);
}

/*
 * Duplicate a CRYPTO_EX_DATA variable - including calling dup() callbacks
 * for each index in the class used by this variable
 */
int CRYPTO_dup_ex_data(int class_index, CRYPTO_EX_DATA *to,
                       CRYPTO_EX_DATA *from)
{
    IMPL_CHECK return EX_IMPL(dup_ex_data) (class_index, to, from);
}

/*
 * Cleanup a CRYPTO_EX_DATA variable - including calling free() callbacks for
 * each index in the class used by this variable
 */
void CRYPTO_free_ex_data(int class_index, void *obj, CRYPTO_EX_DATA *ad)
{
    IMPL_CHECK EX_IMPL(free_ex_data) (class_index, obj, ad);
}

/*
 * For a given CRYPTO_EX_DATA variable, set the value corresponding to a
 * particular index in the class used by this variable
 */
int CRYPTO_set_ex_data(CRYPTO_EX_DATA *ad, int idx, void *val)
{
    int i;

    if (ad->sk == NULL) {
        if ((ad->sk = sk_void_new_null()) == NULL) {
            CRYPTOerr(CRYPTO_F_CRYPTO_SET_EX_DATA, ERR_R_MALLOC_FAILURE);
            return (0);
        }
    }
    i = sk_void_num(ad->sk);

    while (i <= idx) {
        if (!sk_void_push(ad->sk, NULL)) {
            CRYPTOerr(CRYPTO_F_CRYPTO_SET_EX_DATA, ERR_R_MALLOC_FAILURE);
            return (0);
        }
        i++;
    }
    sk_void_set(ad->sk, idx, val);
    return (1);
}

/*
 * For a given CRYPTO_EX_DATA_ variable, get the value corresponding to a
 * particular index in the class used by this variable
 */
void *CRYPTO_get_ex_data(const CRYPTO_EX_DATA *ad, int idx)
{
    if (ad->sk == NULL)
        return (0);
    else if (idx >= sk_void_num(ad->sk))
        return (0);
    else
        return (sk_void_value(ad->sk, idx));
}

IMPLEMENT_STACK_OF(CRYPTO_EX_DATA_FUNCS)
