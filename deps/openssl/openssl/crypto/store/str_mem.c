/* crypto/store/str_mem.c -*- mode:C; c-file-style: "eay" -*- */
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
#include <openssl/err.h>
#include "str_locl.h"

/*
 * The memory store is currently highly experimental.  It's meant to become a
 * base store used by other stores for internal caching (for full caching
 * support, aging needs to be added).
 *
 * The database use is meant to support as much attribute association as
 * possible, while providing for as small search ranges as possible. This is
 * currently provided for by sorting the entries by numbers that are composed
 * of bits set at the positions indicated by attribute type codes.  This
 * provides for ranges determined by the highest attribute type code value.
 * A better idea might be to sort by values computed from the range of
 * attributes associated with the object (basically, the difference between
 * the highest and lowest attribute type code) and it's distance from a base
 * (basically, the lowest associated attribute type code).
 */

typedef struct mem_object_data_st {
    STORE_OBJECT *object;
    STORE_ATTR_INFO *attr_info;
    int references;
} MEM_OBJECT_DATA;

DECLARE_STACK_OF(MEM_OBJECT_DATA)
struct mem_data_st {
    /*
     * sorted with
     * STORE_ATTR_INFO_compare().
     */
    STACK_OF(MEM_OBJECT_DATA) *data;
    /*
     * Currently unused, but can
     * be used to add attributes
     * from parts of the data.
     */
    unsigned int compute_components:1;
};

DECLARE_STACK_OF(STORE_ATTR_INFO)
struct mem_ctx_st {
    /* The type we're searching for */
    int type;
    /*
     * Sets of
     * attributes to search for.  Each
     * element is a STORE_ATTR_INFO.
     */
    STACK_OF(STORE_ATTR_INFO) *search_attributes;
    /*
     * which of the search attributes we
     * found a match for, -1 when we still
     * haven't found any
     */
    int search_index;
    /* -1 as long as we're searching for the first */
    int index;
};

static int mem_init(STORE *s);
static void mem_clean(STORE *s);
static STORE_OBJECT *mem_generate(STORE *s, STORE_OBJECT_TYPES type,
                                  OPENSSL_ITEM attributes[],
                                  OPENSSL_ITEM parameters[]);
static STORE_OBJECT *mem_get(STORE *s, STORE_OBJECT_TYPES type,
                             OPENSSL_ITEM attributes[],
                             OPENSSL_ITEM parameters[]);
static int mem_store(STORE *s, STORE_OBJECT_TYPES type, STORE_OBJECT *data,
                     OPENSSL_ITEM attributes[], OPENSSL_ITEM parameters[]);
static int mem_modify(STORE *s, STORE_OBJECT_TYPES type,
                      OPENSSL_ITEM search_attributes[],
                      OPENSSL_ITEM add_attributes[],
                      OPENSSL_ITEM modify_attributes[],
                      OPENSSL_ITEM delete_attributes[],
                      OPENSSL_ITEM parameters[]);
static int mem_delete(STORE *s, STORE_OBJECT_TYPES type,
                      OPENSSL_ITEM attributes[], OPENSSL_ITEM parameters[]);
static void *mem_list_start(STORE *s, STORE_OBJECT_TYPES type,
                            OPENSSL_ITEM attributes[],
                            OPENSSL_ITEM parameters[]);
static STORE_OBJECT *mem_list_next(STORE *s, void *handle);
static int mem_list_end(STORE *s, void *handle);
static int mem_list_endp(STORE *s, void *handle);
static int mem_lock(STORE *s, OPENSSL_ITEM attributes[],
                    OPENSSL_ITEM parameters[]);
static int mem_unlock(STORE *s, OPENSSL_ITEM attributes[],
                      OPENSSL_ITEM parameters[]);
static int mem_ctrl(STORE *s, int cmd, long l, void *p, void (*f) (void));

static STORE_METHOD store_memory = {
    "OpenSSL memory store interface",
    mem_init,
    mem_clean,
    mem_generate,
    mem_get,
    mem_store,
    mem_modify,
    NULL,                       /* revoke */
    mem_delete,
    mem_list_start,
    mem_list_next,
    mem_list_end,
    mem_list_endp,
    NULL,                       /* update */
    mem_lock,
    mem_unlock,
    mem_ctrl
};

const STORE_METHOD *STORE_Memory(void)
{
    return &store_memory;
}

static int mem_init(STORE *s)
{
    return 1;
}

static void mem_clean(STORE *s)
{
    return;
}

static STORE_OBJECT *mem_generate(STORE *s, STORE_OBJECT_TYPES type,
                                  OPENSSL_ITEM attributes[],
                                  OPENSSL_ITEM parameters[])
{
    STOREerr(STORE_F_MEM_GENERATE, STORE_R_NOT_IMPLEMENTED);
    return 0;
}

static STORE_OBJECT *mem_get(STORE *s, STORE_OBJECT_TYPES type,
                             OPENSSL_ITEM attributes[],
                             OPENSSL_ITEM parameters[])
{
    void *context = mem_list_start(s, type, attributes, parameters);

    if (context) {
        STORE_OBJECT *object = mem_list_next(s, context);

        if (mem_list_end(s, context))
            return object;
    }
    return NULL;
}

static int mem_store(STORE *s, STORE_OBJECT_TYPES type,
                     STORE_OBJECT *data, OPENSSL_ITEM attributes[],
                     OPENSSL_ITEM parameters[])
{
    STOREerr(STORE_F_MEM_STORE, STORE_R_NOT_IMPLEMENTED);
    return 0;
}

static int mem_modify(STORE *s, STORE_OBJECT_TYPES type,
                      OPENSSL_ITEM search_attributes[],
                      OPENSSL_ITEM add_attributes[],
                      OPENSSL_ITEM modify_attributes[],
                      OPENSSL_ITEM delete_attributes[],
                      OPENSSL_ITEM parameters[])
{
    STOREerr(STORE_F_MEM_MODIFY, STORE_R_NOT_IMPLEMENTED);
    return 0;
}

static int mem_delete(STORE *s, STORE_OBJECT_TYPES type,
                      OPENSSL_ITEM attributes[], OPENSSL_ITEM parameters[])
{
    STOREerr(STORE_F_MEM_DELETE, STORE_R_NOT_IMPLEMENTED);
    return 0;
}

/*
 * The list functions may be the hardest to understand.  Basically,
 * mem_list_start compiles a stack of attribute info elements, and puts that
 * stack into the context to be returned.  mem_list_next will then find the
 * first matching element in the store, and then walk all the way to the end
 * of the store (since any combination of attribute bits above the starting
 * point may match the searched for bit pattern...).
 */
static void *mem_list_start(STORE *s, STORE_OBJECT_TYPES type,
                            OPENSSL_ITEM attributes[],
                            OPENSSL_ITEM parameters[])
{
    struct mem_ctx_st *context =
        (struct mem_ctx_st *)OPENSSL_malloc(sizeof(struct mem_ctx_st));
    void *attribute_context = NULL;
    STORE_ATTR_INFO *attrs = NULL;

    if (!context) {
        STOREerr(STORE_F_MEM_LIST_START, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    memset(context, 0, sizeof(struct mem_ctx_st));

    attribute_context = STORE_parse_attrs_start(attributes);
    if (!attribute_context) {
        STOREerr(STORE_F_MEM_LIST_START, ERR_R_STORE_LIB);
        goto err;
    }

    while ((attrs = STORE_parse_attrs_next(attribute_context))) {
        if (context->search_attributes == NULL) {
            context->search_attributes =
                sk_STORE_ATTR_INFO_new(STORE_ATTR_INFO_compare);
            if (!context->search_attributes) {
                STOREerr(STORE_F_MEM_LIST_START, ERR_R_MALLOC_FAILURE);
                goto err;
            }
        }
        sk_STORE_ATTR_INFO_push(context->search_attributes, attrs);
    }
    if (!STORE_parse_attrs_endp(attribute_context))
        goto err;
    STORE_parse_attrs_end(attribute_context);
    context->search_index = -1;
    context->index = -1;
    return context;
 err:
    if (attribute_context)
        STORE_parse_attrs_end(attribute_context);
    mem_list_end(s, context);
    return NULL;
}

static STORE_OBJECT *mem_list_next(STORE *s, void *handle)
{
    int i;
    struct mem_ctx_st *context = (struct mem_ctx_st *)handle;
    struct mem_object_data_st key = { 0, 0, 1 };
    struct mem_data_st *store = (struct mem_data_st *)STORE_get_ex_data(s, 1);
    int srch;
    int cres = 0;

    if (!context) {
        STOREerr(STORE_F_MEM_LIST_NEXT, ERR_R_PASSED_NULL_PARAMETER);
        return NULL;
    }
    if (!store) {
        STOREerr(STORE_F_MEM_LIST_NEXT, STORE_R_NO_STORE);
        return NULL;
    }

    if (context->search_index == -1) {
        for (i = 0;
             i < sk_STORE_ATTR_INFO_num(context->search_attributes); i++) {
            key.attr_info
                = sk_STORE_ATTR_INFO_value(context->search_attributes, i);
            srch = sk_MEM_OBJECT_DATA_find_ex(store->data, &key);

            if (srch >= 0) {
                context->search_index = srch;
                break;
            }
        }
    }
    if (context->search_index < 0)
        return NULL;

    key.attr_info =
        sk_STORE_ATTR_INFO_value(context->search_attributes,
                                 context->search_index);
    for (srch = context->search_index;
         srch < sk_MEM_OBJECT_DATA_num(store->data)
         && STORE_ATTR_INFO_in_range(key.attr_info,
                                     sk_MEM_OBJECT_DATA_value(store->data,
                                                              srch)->attr_info)
         && !(cres =
              STORE_ATTR_INFO_in_ex(key.attr_info,
                                    sk_MEM_OBJECT_DATA_value(store->data,
                                                             srch)->attr_info));
         srch++) ;

    context->search_index = srch;
    if (cres)
        return (sk_MEM_OBJECT_DATA_value(store->data, srch))->object;
    return NULL;
}

static int mem_list_end(STORE *s, void *handle)
{
    struct mem_ctx_st *context = (struct mem_ctx_st *)handle;

    if (!context) {
        STOREerr(STORE_F_MEM_LIST_END, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    if (context && context->search_attributes)
        sk_STORE_ATTR_INFO_free(context->search_attributes);
    if (context)
        OPENSSL_free(context);
    return 1;
}

static int mem_list_endp(STORE *s, void *handle)
{
    struct mem_ctx_st *context = (struct mem_ctx_st *)handle;

    if (!context
        || context->search_index
        == sk_STORE_ATTR_INFO_num(context->search_attributes))
        return 1;
    return 0;
}

static int mem_lock(STORE *s, OPENSSL_ITEM attributes[],
                    OPENSSL_ITEM parameters[])
{
    return 1;
}

static int mem_unlock(STORE *s, OPENSSL_ITEM attributes[],
                      OPENSSL_ITEM parameters[])
{
    return 1;
}

static int mem_ctrl(STORE *s, int cmd, long l, void *p, void (*f) (void))
{
    return 1;
}
