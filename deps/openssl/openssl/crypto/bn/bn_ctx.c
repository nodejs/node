/* crypto/bn/bn_ctx.c */
/* Written by Ulf Moeller for the OpenSSL project. */
/* ====================================================================
 * Copyright (c) 1998-2004 The OpenSSL Project.  All rights reserved.
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

#if !defined(BN_CTX_DEBUG) && !defined(BN_DEBUG)
# ifndef NDEBUG
#  define NDEBUG
# endif
#endif

#include <stdio.h>
#include <assert.h>

#include "cryptlib.h"
#include "bn_lcl.h"

/*-
 * TODO list
 *
 * 1. Check a bunch of "(words+1)" type hacks in various bignum functions and
 * check they can be safely removed.
 *  - Check +1 and other ugliness in BN_from_montgomery()
 *
 * 2. Consider allowing a BN_new_ex() that, at least, lets you specify an
 * appropriate 'block' size that will be honoured by bn_expand_internal() to
 * prevent piddly little reallocations. OTOH, profiling bignum expansions in
 * BN_CTX doesn't show this to be a big issue.
 */

/* How many bignums are in each "pool item"; */
#define BN_CTX_POOL_SIZE        16
/* The stack frame info is resizing, set a first-time expansion size; */
#define BN_CTX_START_FRAMES     32

/***********/
/* BN_POOL */
/***********/

/* A bundle of bignums that can be linked with other bundles */
typedef struct bignum_pool_item {
    /* The bignum values */
    BIGNUM vals[BN_CTX_POOL_SIZE];
    /* Linked-list admin */
    struct bignum_pool_item *prev, *next;
} BN_POOL_ITEM;
/* A linked-list of bignums grouped in bundles */
typedef struct bignum_pool {
    /* Linked-list admin */
    BN_POOL_ITEM *head, *current, *tail;
    /* Stack depth and allocation size */
    unsigned used, size;
} BN_POOL;
static void BN_POOL_init(BN_POOL *);
static void BN_POOL_finish(BN_POOL *);
#ifndef OPENSSL_NO_DEPRECATED
static void BN_POOL_reset(BN_POOL *);
#endif
static BIGNUM *BN_POOL_get(BN_POOL *);
static void BN_POOL_release(BN_POOL *, unsigned int);

/************/
/* BN_STACK */
/************/

/* A wrapper to manage the "stack frames" */
typedef struct bignum_ctx_stack {
    /* Array of indexes into the bignum stack */
    unsigned int *indexes;
    /* Number of stack frames, and the size of the allocated array */
    unsigned int depth, size;
} BN_STACK;
static void BN_STACK_init(BN_STACK *);
static void BN_STACK_finish(BN_STACK *);
#ifndef OPENSSL_NO_DEPRECATED
static void BN_STACK_reset(BN_STACK *);
#endif
static int BN_STACK_push(BN_STACK *, unsigned int);
static unsigned int BN_STACK_pop(BN_STACK *);

/**********/
/* BN_CTX */
/**********/

/* The opaque BN_CTX type */
struct bignum_ctx {
    /* The bignum bundles */
    BN_POOL pool;
    /* The "stack frames", if you will */
    BN_STACK stack;
    /* The number of bignums currently assigned */
    unsigned int used;
    /* Depth of stack overflow */
    int err_stack;
    /* Block "gets" until an "end" (compatibility behaviour) */
    int too_many;
};

/* Enable this to find BN_CTX bugs */
#ifdef BN_CTX_DEBUG
static const char *ctxdbg_cur = NULL;
static void ctxdbg(BN_CTX *ctx)
{
    unsigned int bnidx = 0, fpidx = 0;
    BN_POOL_ITEM *item = ctx->pool.head;
    BN_STACK *stack = &ctx->stack;
    fprintf(stderr, "(%16p): ", ctx);
    while (bnidx < ctx->used) {
        fprintf(stderr, "%03x ", item->vals[bnidx++ % BN_CTX_POOL_SIZE].dmax);
        if (!(bnidx % BN_CTX_POOL_SIZE))
            item = item->next;
    }
    fprintf(stderr, "\n");
    bnidx = 0;
    fprintf(stderr, "          : ");
    while (fpidx < stack->depth) {
        while (bnidx++ < stack->indexes[fpidx])
            fprintf(stderr, "    ");
        fprintf(stderr, "^^^ ");
        bnidx++;
        fpidx++;
    }
    fprintf(stderr, "\n");
}

# define CTXDBG_ENTRY(str, ctx)  do { \
                                ctxdbg_cur = (str); \
                                fprintf(stderr,"Starting %s\n", ctxdbg_cur); \
                                ctxdbg(ctx); \
                                } while(0)
# define CTXDBG_EXIT(ctx)        do { \
                                fprintf(stderr,"Ending %s\n", ctxdbg_cur); \
                                ctxdbg(ctx); \
                                } while(0)
# define CTXDBG_RET(ctx,ret)
#else
# define CTXDBG_ENTRY(str, ctx)
# define CTXDBG_EXIT(ctx)
# define CTXDBG_RET(ctx,ret)
#endif

/*
 * This function is an evil legacy and should not be used. This
 * implementation is WYSIWYG, though I've done my best.
 */
#ifndef OPENSSL_NO_DEPRECATED
void BN_CTX_init(BN_CTX *ctx)
{
    /*
     * Assume the caller obtained the context via BN_CTX_new() and so is
     * trying to reset it for use. Nothing else makes sense, least of all
     * binary compatibility from a time when they could declare a static
     * variable.
     */
    BN_POOL_reset(&ctx->pool);
    BN_STACK_reset(&ctx->stack);
    ctx->used = 0;
    ctx->err_stack = 0;
    ctx->too_many = 0;
}
#endif

BN_CTX *BN_CTX_new(void)
{
    BN_CTX *ret = OPENSSL_malloc(sizeof(BN_CTX));
    if (!ret) {
        BNerr(BN_F_BN_CTX_NEW, ERR_R_MALLOC_FAILURE);
        return NULL;
    }
    /* Initialise the structure */
    BN_POOL_init(&ret->pool);
    BN_STACK_init(&ret->stack);
    ret->used = 0;
    ret->err_stack = 0;
    ret->too_many = 0;
    return ret;
}

void BN_CTX_free(BN_CTX *ctx)
{
    if (ctx == NULL)
        return;
#ifdef BN_CTX_DEBUG
    {
        BN_POOL_ITEM *pool = ctx->pool.head;
        fprintf(stderr, "BN_CTX_free, stack-size=%d, pool-bignums=%d\n",
                ctx->stack.size, ctx->pool.size);
        fprintf(stderr, "dmaxs: ");
        while (pool) {
            unsigned loop = 0;
            while (loop < BN_CTX_POOL_SIZE)
                fprintf(stderr, "%02x ", pool->vals[loop++].dmax);
            pool = pool->next;
        }
        fprintf(stderr, "\n");
    }
#endif
    BN_STACK_finish(&ctx->stack);
    BN_POOL_finish(&ctx->pool);
    OPENSSL_free(ctx);
}

void BN_CTX_start(BN_CTX *ctx)
{
    CTXDBG_ENTRY("BN_CTX_start", ctx);
    /* If we're already overflowing ... */
    if (ctx->err_stack || ctx->too_many)
        ctx->err_stack++;
    /* (Try to) get a new frame pointer */
    else if (!BN_STACK_push(&ctx->stack, ctx->used)) {
        BNerr(BN_F_BN_CTX_START, BN_R_TOO_MANY_TEMPORARY_VARIABLES);
        ctx->err_stack++;
    }
    CTXDBG_EXIT(ctx);
}

void BN_CTX_end(BN_CTX *ctx)
{
    CTXDBG_ENTRY("BN_CTX_end", ctx);
    if (ctx->err_stack)
        ctx->err_stack--;
    else {
        unsigned int fp = BN_STACK_pop(&ctx->stack);
        /* Does this stack frame have anything to release? */
        if (fp < ctx->used)
            BN_POOL_release(&ctx->pool, ctx->used - fp);
        ctx->used = fp;
        /* Unjam "too_many" in case "get" had failed */
        ctx->too_many = 0;
    }
    CTXDBG_EXIT(ctx);
}

BIGNUM *BN_CTX_get(BN_CTX *ctx)
{
    BIGNUM *ret;
    CTXDBG_ENTRY("BN_CTX_get", ctx);
    if (ctx->err_stack || ctx->too_many)
        return NULL;
    if ((ret = BN_POOL_get(&ctx->pool)) == NULL) {
        /*
         * Setting too_many prevents repeated "get" attempts from cluttering
         * the error stack.
         */
        ctx->too_many = 1;
        BNerr(BN_F_BN_CTX_GET, BN_R_TOO_MANY_TEMPORARY_VARIABLES);
        return NULL;
    }
    /* OK, make sure the returned bignum is "zero" */
    BN_zero(ret);
    ctx->used++;
    CTXDBG_RET(ctx, ret);
    return ret;
}

/************/
/* BN_STACK */
/************/

static void BN_STACK_init(BN_STACK *st)
{
    st->indexes = NULL;
    st->depth = st->size = 0;
}

static void BN_STACK_finish(BN_STACK *st)
{
    if (st->size)
        OPENSSL_free(st->indexes);
}

#ifndef OPENSSL_NO_DEPRECATED
static void BN_STACK_reset(BN_STACK *st)
{
    st->depth = 0;
}
#endif

static int BN_STACK_push(BN_STACK *st, unsigned int idx)
{
    if (st->depth == st->size)
        /* Need to expand */
    {
        unsigned int newsize = (st->size ?
                                (st->size * 3 / 2) : BN_CTX_START_FRAMES);
        unsigned int *newitems = OPENSSL_malloc(newsize *
                                                sizeof(unsigned int));
        if (!newitems)
            return 0;
        if (st->depth)
            memcpy(newitems, st->indexes, st->depth * sizeof(unsigned int));
        if (st->size)
            OPENSSL_free(st->indexes);
        st->indexes = newitems;
        st->size = newsize;
    }
    st->indexes[(st->depth)++] = idx;
    return 1;
}

static unsigned int BN_STACK_pop(BN_STACK *st)
{
    return st->indexes[--(st->depth)];
}

/***********/
/* BN_POOL */
/***********/

static void BN_POOL_init(BN_POOL *p)
{
    p->head = p->current = p->tail = NULL;
    p->used = p->size = 0;
}

static void BN_POOL_finish(BN_POOL *p)
{
    while (p->head) {
        unsigned int loop = 0;
        BIGNUM *bn = p->head->vals;
        while (loop++ < BN_CTX_POOL_SIZE) {
            if (bn->d)
                BN_clear_free(bn);
            bn++;
        }
        p->current = p->head->next;
        OPENSSL_free(p->head);
        p->head = p->current;
    }
}

#ifndef OPENSSL_NO_DEPRECATED
static void BN_POOL_reset(BN_POOL *p)
{
    BN_POOL_ITEM *item = p->head;
    while (item) {
        unsigned int loop = 0;
        BIGNUM *bn = item->vals;
        while (loop++ < BN_CTX_POOL_SIZE) {
            if (bn->d)
                BN_clear(bn);
            bn++;
        }
        item = item->next;
    }
    p->current = p->head;
    p->used = 0;
}
#endif

static BIGNUM *BN_POOL_get(BN_POOL *p)
{
    if (p->used == p->size) {
        BIGNUM *bn;
        unsigned int loop = 0;
        BN_POOL_ITEM *item = OPENSSL_malloc(sizeof(BN_POOL_ITEM));
        if (!item)
            return NULL;
        /* Initialise the structure */
        bn = item->vals;
        while (loop++ < BN_CTX_POOL_SIZE)
            BN_init(bn++);
        item->prev = p->tail;
        item->next = NULL;
        /* Link it in */
        if (!p->head)
            p->head = p->current = p->tail = item;
        else {
            p->tail->next = item;
            p->tail = item;
            p->current = item;
        }
        p->size += BN_CTX_POOL_SIZE;
        p->used++;
        /* Return the first bignum from the new pool */
        return item->vals;
    }
    if (!p->used)
        p->current = p->head;
    else if ((p->used % BN_CTX_POOL_SIZE) == 0)
        p->current = p->current->next;
    return p->current->vals + ((p->used++) % BN_CTX_POOL_SIZE);
}

static void BN_POOL_release(BN_POOL *p, unsigned int num)
{
    unsigned int offset = (p->used - 1) % BN_CTX_POOL_SIZE;
    p->used -= num;
    while (num--) {
        bn_check_top(p->current->vals + offset);
        if (!offset) {
            offset = BN_CTX_POOL_SIZE - 1;
            p->current = p->current->prev;
        } else
            offset--;
    }
}
