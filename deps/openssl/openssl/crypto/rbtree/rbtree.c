/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2016 David Gwynne <david@gwynne.id.au>
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * The code here comes from David Gwynne <david@gwynne.id.au>. The original
 * version can be found:
 *    https://github.com/dgwynne/data-structures/
 * file bst.h. The same code is also part of OpenBSD OS where it is shipped
 * under BSD license.
 *
 * David Gwynne agrees to include modified version to OpenSSL and ship it
 * under OpenSSL Apache 2.0 license.
 */

#include "internal/ossl_rbtree.h"

#ifndef NDEBUG
#include <assert.h>
#endif

#define OSSL_RBT_BLACK 0
#define OSSL_RBT_RED 1

static struct ossl_rbt_entry *
rbt_n2e(const struct ossl_rbt_type *t, void *node)
{
    uintptr_t addr = (uintptr_t)node;

    return (struct ossl_rbt_entry *)(addr + t->t_offset);
}

static void *
rbt_e2n(const struct ossl_rbt_type *t, struct ossl_rbt_entry *rbe)
{
    uintptr_t addr = (uintptr_t)rbe;

    return (void *)(addr - t->t_offset);
}

#define OSSL_RBE_LEFT(_rbe) (_rbe)->rb_left
#define OSSL_RBE_RIGHT(_rbe) (_rbe)->rb_right
#define OSSL_RBE_PARENT(_rbe) (_rbe)->rb_parent
#define OSSL_RBE_COLOR(_rbe) (_rbe)->rb_color

#define OSSL_RBH_ROOT(_rbt) (_rbt)->rb_root

static void
rbe_set(struct ossl_rbt_entry *rbe, struct ossl_rbt_entry *parent)
{
    OSSL_RBE_PARENT(rbe) = parent;
    OSSL_RBE_LEFT(rbe) = OSSL_RBE_RIGHT(rbe) = NULL;
    OSSL_RBE_COLOR(rbe) = OSSL_RBT_RED;
}

static void
rbe_set_blackred(struct ossl_rbt_entry *black, struct ossl_rbt_entry *red)
{
    OSSL_RBE_COLOR(black) = OSSL_RBT_BLACK;
    OSSL_RBE_COLOR(red) = OSSL_RBT_RED;
}

static void
rbe_rotate_left(struct ossl_rbt_tree *rbt, struct ossl_rbt_entry *rbe)
{
    struct ossl_rbt_entry *parent;
    struct ossl_rbt_entry *tmp;

    tmp = OSSL_RBE_RIGHT(rbe);
    OSSL_RBE_RIGHT(rbe) = OSSL_RBE_LEFT(tmp);
    if (OSSL_RBE_RIGHT(rbe) != NULL)
        OSSL_RBE_PARENT(OSSL_RBE_LEFT(tmp)) = rbe;

    parent = OSSL_RBE_PARENT(rbe);
    OSSL_RBE_PARENT(tmp) = parent;
    if (parent != NULL) {
        if (rbe == OSSL_RBE_LEFT(parent))
            OSSL_RBE_LEFT(parent) = tmp;
        else
            OSSL_RBE_RIGHT(parent) = tmp;
    } else
        OSSL_RBH_ROOT(rbt) = tmp;

    OSSL_RBE_LEFT(tmp) = rbe;
    OSSL_RBE_PARENT(rbe) = tmp;
}

static void
rbe_rotate_right(struct ossl_rbt_tree *rbt, struct ossl_rbt_entry *rbe)
{
    struct ossl_rbt_entry *parent;
    struct ossl_rbt_entry *tmp;

    tmp = OSSL_RBE_LEFT(rbe);
    OSSL_RBE_LEFT(rbe) = OSSL_RBE_RIGHT(tmp);
    if (OSSL_RBE_LEFT(rbe) != NULL)
        OSSL_RBE_PARENT(OSSL_RBE_RIGHT(tmp)) = rbe;

    parent = OSSL_RBE_PARENT(rbe);
    OSSL_RBE_PARENT(tmp) = parent;
    if (parent != NULL) {
        if (rbe == OSSL_RBE_LEFT(parent))
            OSSL_RBE_LEFT(parent) = tmp;
        else
            OSSL_RBE_RIGHT(parent) = tmp;
    } else
        OSSL_RBH_ROOT(rbt) = tmp;

    OSSL_RBE_RIGHT(tmp) = rbe;
    OSSL_RBE_PARENT(rbe) = tmp;
}

static void
rbe_insert_color(struct ossl_rbt_tree *rbt, struct ossl_rbt_entry *rbe)
{
    struct ossl_rbt_entry *parent, *gparent, *tmp;

    while ((parent = OSSL_RBE_PARENT(rbe)) != NULL && OSSL_RBE_COLOR(parent) == OSSL_RBT_RED) {
        gparent = OSSL_RBE_PARENT(parent);

        if (parent == OSSL_RBE_LEFT(gparent)) {
            tmp = OSSL_RBE_RIGHT(gparent);
            if (tmp != NULL && OSSL_RBE_COLOR(tmp) == OSSL_RBT_RED) {
                OSSL_RBE_COLOR(tmp) = OSSL_RBT_BLACK;
                rbe_set_blackred(parent, gparent);
                rbe = gparent;
                continue;
            }

            if (OSSL_RBE_RIGHT(parent) == rbe) {
                rbe_rotate_left(rbt, parent);
                tmp = parent;
                parent = rbe;
                rbe = tmp;
            }

            rbe_set_blackred(parent, gparent);
            rbe_rotate_right(rbt, gparent);
        } else {
            tmp = OSSL_RBE_LEFT(gparent);
            if (tmp != NULL && OSSL_RBE_COLOR(tmp) == OSSL_RBT_RED) {
                OSSL_RBE_COLOR(tmp) = OSSL_RBT_BLACK;
                rbe_set_blackred(parent, gparent);
                rbe = gparent;
                continue;
            }

            if (OSSL_RBE_LEFT(parent) == rbe) {
                rbe_rotate_right(rbt, parent);
                tmp = parent;
                parent = rbe;
                rbe = tmp;
            }

            rbe_set_blackred(parent, gparent);
            rbe_rotate_left(rbt, gparent);
        }
    }

    OSSL_RBE_COLOR(OSSL_RBH_ROOT(rbt)) = OSSL_RBT_BLACK;
}

static void
rbe_remove_color(struct ossl_rbt_tree *rbt,
    struct ossl_rbt_entry *parent, struct ossl_rbt_entry *rbe)
{
    struct ossl_rbt_entry *tmp;

    while ((rbe == NULL || OSSL_RBE_COLOR(rbe) == OSSL_RBT_BLACK) && rbe != OSSL_RBH_ROOT(rbt)) {
        if (OSSL_RBE_LEFT(parent) == rbe) {
            tmp = OSSL_RBE_RIGHT(parent);
            if (OSSL_RBE_COLOR(tmp) == OSSL_RBT_RED) {
                rbe_set_blackred(tmp, parent);
                rbe_rotate_left(rbt, parent);
                tmp = OSSL_RBE_RIGHT(parent);
            }
            if ((OSSL_RBE_LEFT(tmp) == NULL || OSSL_RBE_COLOR(OSSL_RBE_LEFT(tmp)) == OSSL_RBT_BLACK) && (OSSL_RBE_RIGHT(tmp) == NULL || OSSL_RBE_COLOR(OSSL_RBE_RIGHT(tmp)) == OSSL_RBT_BLACK)) {
                OSSL_RBE_COLOR(tmp) = OSSL_RBT_RED;
                rbe = parent;
                parent = OSSL_RBE_PARENT(rbe);
            } else {
                if (OSSL_RBE_RIGHT(tmp) == NULL || OSSL_RBE_COLOR(OSSL_RBE_RIGHT(tmp)) == OSSL_RBT_BLACK) {
                    struct ossl_rbt_entry *oleft;

                    oleft = OSSL_RBE_LEFT(tmp);
                    if (oleft != NULL)
                        OSSL_RBE_COLOR(oleft) = OSSL_RBT_BLACK;

                    OSSL_RBE_COLOR(tmp) = OSSL_RBT_RED;
                    rbe_rotate_right(rbt, tmp);
                    tmp = OSSL_RBE_RIGHT(parent);
                }

                OSSL_RBE_COLOR(tmp) = OSSL_RBE_COLOR(parent);
                OSSL_RBE_COLOR(parent) = OSSL_RBT_BLACK;
                if (OSSL_RBE_RIGHT(tmp))
                    OSSL_RBE_COLOR(OSSL_RBE_RIGHT(tmp)) = OSSL_RBT_BLACK;

                rbe_rotate_left(rbt, parent);
                rbe = OSSL_RBH_ROOT(rbt);
                break;
            }
        } else {
            tmp = OSSL_RBE_LEFT(parent);
            if (OSSL_RBE_COLOR(tmp) == OSSL_RBT_RED) {
                rbe_set_blackred(tmp, parent);
                rbe_rotate_right(rbt, parent);
                tmp = OSSL_RBE_LEFT(parent);
            }

            if ((OSSL_RBE_LEFT(tmp) == NULL || OSSL_RBE_COLOR(OSSL_RBE_LEFT(tmp)) == OSSL_RBT_BLACK) && (OSSL_RBE_RIGHT(tmp) == NULL || OSSL_RBE_COLOR(OSSL_RBE_RIGHT(tmp)) == OSSL_RBT_BLACK)) {
                OSSL_RBE_COLOR(tmp) = OSSL_RBT_RED;
                rbe = parent;
                parent = OSSL_RBE_PARENT(rbe);
            } else {
                if (OSSL_RBE_LEFT(tmp) == NULL || OSSL_RBE_COLOR(OSSL_RBE_LEFT(tmp)) == OSSL_RBT_BLACK) {
                    struct ossl_rbt_entry *oright;

                    oright = OSSL_RBE_RIGHT(tmp);
                    if (oright != NULL)
                        OSSL_RBE_COLOR(oright) = OSSL_RBT_BLACK;

                    OSSL_RBE_COLOR(tmp) = OSSL_RBT_RED;
                    rbe_rotate_left(rbt, tmp);
                    tmp = OSSL_RBE_LEFT(parent);
                }

                OSSL_RBE_COLOR(tmp) = OSSL_RBE_COLOR(parent);
                OSSL_RBE_COLOR(parent) = OSSL_RBT_BLACK;
                if (OSSL_RBE_LEFT(tmp) != NULL)
                    OSSL_RBE_COLOR(OSSL_RBE_LEFT(tmp)) = OSSL_RBT_BLACK;

                rbe_rotate_right(rbt, parent);
                rbe = OSSL_RBH_ROOT(rbt);
                break;
            }
        }
    }

    if (rbe != NULL)
        OSSL_RBE_COLOR(rbe) = OSSL_RBT_BLACK;
}

static struct ossl_rbt_entry *
rbe_remove(struct ossl_rbt_tree *rbt, struct ossl_rbt_entry *rbe)
{
    struct ossl_rbt_entry *child, *parent, *old = rbe;
    unsigned int color;

    if (OSSL_RBE_LEFT(rbe) == NULL)
        child = OSSL_RBE_RIGHT(rbe);
    else if (OSSL_RBE_RIGHT(rbe) == NULL)
        child = OSSL_RBE_LEFT(rbe);
    else {
        struct ossl_rbt_entry *tmp;

        rbe = OSSL_RBE_RIGHT(rbe);
        while ((tmp = OSSL_RBE_LEFT(rbe)) != NULL)
            rbe = tmp;

        child = OSSL_RBE_RIGHT(rbe);
        parent = OSSL_RBE_PARENT(rbe);
        color = OSSL_RBE_COLOR(rbe);
        if (child != NULL)
            OSSL_RBE_PARENT(child) = parent;
        if (parent != NULL) {
            if (OSSL_RBE_LEFT(parent) == rbe)
                OSSL_RBE_LEFT(parent) = child;
            else
                OSSL_RBE_RIGHT(parent) = child;
        } else
            OSSL_RBH_ROOT(rbt) = child;
        if (OSSL_RBE_PARENT(rbe) == old)
            parent = rbe;
        *rbe = *old;

        tmp = OSSL_RBE_PARENT(old);
        if (tmp != NULL) {
            if (OSSL_RBE_LEFT(tmp) == old)
                OSSL_RBE_LEFT(tmp) = rbe;
            else
                OSSL_RBE_RIGHT(tmp) = rbe;
        } else
            OSSL_RBH_ROOT(rbt) = rbe;

        OSSL_RBE_PARENT(OSSL_RBE_LEFT(old)) = rbe;
        if (OSSL_RBE_RIGHT(old))
            OSSL_RBE_PARENT(OSSL_RBE_RIGHT(old)) = rbe;
        goto color;
    }

    parent = OSSL_RBE_PARENT(rbe);
    color = OSSL_RBE_COLOR(rbe);

    if (child != NULL)
        OSSL_RBE_PARENT(child) = parent;
    if (parent != NULL) {
        if (OSSL_RBE_LEFT(parent) == rbe)
            OSSL_RBE_LEFT(parent) = child;
        else
            OSSL_RBE_RIGHT(parent) = child;
    } else
        OSSL_RBH_ROOT(rbt) = child;
color:
    if (color == OSSL_RBT_BLACK)
        rbe_remove_color(rbt, parent, child);

#ifndef NDEBUG
    if (old != NULL) {
        OSSL_RBE_PARENT(old) = NULL;
        OSSL_RBE_LEFT(old) = NULL;
        OSSL_RBE_RIGHT(old) = NULL;
    }
#endif

    return old;
}

void *
ossl_rbt_remove(const struct ossl_rbt_type *t, struct ossl_rbt_tree *rbt, void *elm)
{
    struct ossl_rbt_entry *rbe = rbt_n2e(t, elm);
    struct ossl_rbt_entry *old;

    old = rbe_remove(rbt, rbe);

    return old == NULL ? NULL : rbt_e2n(t, old);
}

void *
ossl_rbt_insert(const struct ossl_rbt_type *t, struct ossl_rbt_tree *rbt, void *elm)
{
    struct ossl_rbt_entry *rbe = rbt_n2e(t, elm);
    struct ossl_rbt_entry *tmp;
    struct ossl_rbt_entry *parent = NULL;
    void *node;
    int comp = 0;

#ifndef NDEBUG
    assert(rbe->rb_parent == NULL);
    assert(rbe->rb_left == NULL);
    assert(rbe->rb_right == NULL);
#endif

    tmp = OSSL_RBH_ROOT(rbt);
    while (tmp != NULL) {
        parent = tmp;

        node = rbt_e2n(t, tmp);
        comp = (*t->t_compare)(elm, node);
        if (comp < 0)
            tmp = OSSL_RBE_LEFT(tmp);
        else if (comp > 0)
            tmp = OSSL_RBE_RIGHT(tmp);
        else
            return node;
    }

    rbe_set(rbe, parent);

    if (parent != NULL) {
        if (comp < 0)
            OSSL_RBE_LEFT(parent) = rbe;
        else
            OSSL_RBE_RIGHT(parent) = rbe;
    } else
        OSSL_RBH_ROOT(rbt) = rbe;

    rbe_insert_color(rbt, rbe);

    return NULL;
}

/* Finds the node with the same key as elm */
void *
ossl_rbt_find(const struct ossl_rbt_type *t, struct ossl_rbt_tree *rbt, const void *key)
{
    struct ossl_rbt_entry *tmp = OSSL_RBH_ROOT(rbt);
    void *node;
    int comp;

    while (tmp != NULL) {
        node = rbt_e2n(t, tmp);
        comp = (*t->t_compare)(key, node);
        if (comp < 0)
            tmp = OSSL_RBE_LEFT(tmp);
        else if (comp > 0)
            tmp = OSSL_RBE_RIGHT(tmp);
        else
            return node;
    }

    return NULL;
}

/* Finds the first node greater than or equal to the search key */
void *
ossl_rbt_nfind(const struct ossl_rbt_type *t, struct ossl_rbt_tree *rbt, const void *key)
{
    struct ossl_rbt_entry *tmp = OSSL_RBH_ROOT(rbt);
    void *node;
    void *res = NULL;
    int comp;

    while (tmp != NULL) {
        node = rbt_e2n(t, tmp);
        comp = (*t->t_compare)(key, node);
        if (comp < 0) {
            res = node;
            tmp = OSSL_RBE_LEFT(tmp);
        } else if (comp > 0)
            tmp = OSSL_RBE_RIGHT(tmp);
        else
            return node;
    }

    return res;
}

void *
ossl_rbt_next(const struct ossl_rbt_type *t, void *elm)
{
    struct ossl_rbt_entry *rbe = rbt_n2e(t, elm);

    if (OSSL_RBE_RIGHT(rbe) != NULL) {
        rbe = OSSL_RBE_RIGHT(rbe);
        while (OSSL_RBE_LEFT(rbe) != NULL)
            rbe = OSSL_RBE_LEFT(rbe);
    } else {
        if (OSSL_RBE_PARENT(rbe) && (rbe == OSSL_RBE_LEFT(OSSL_RBE_PARENT(rbe))))
            rbe = OSSL_RBE_PARENT(rbe);
        else {
            while (OSSL_RBE_PARENT(rbe) && (rbe == OSSL_RBE_RIGHT(OSSL_RBE_PARENT(rbe))))
                rbe = OSSL_RBE_PARENT(rbe);
            rbe = OSSL_RBE_PARENT(rbe);
        }
    }

    return rbe == NULL ? NULL : rbt_e2n(t, rbe);
}

void *
ossl_rbt_prev(const struct ossl_rbt_type *t, void *elm)
{
    struct ossl_rbt_entry *rbe = rbt_n2e(t, elm);

    if (OSSL_RBE_LEFT(rbe)) {
        rbe = OSSL_RBE_LEFT(rbe);
        while (OSSL_RBE_RIGHT(rbe))
            rbe = OSSL_RBE_RIGHT(rbe);
    } else {
        if (OSSL_RBE_PARENT(rbe) && (rbe == OSSL_RBE_RIGHT(OSSL_RBE_PARENT(rbe))))
            rbe = OSSL_RBE_PARENT(rbe);
        else {
            while (OSSL_RBE_PARENT(rbe) && (rbe == OSSL_RBE_LEFT(OSSL_RBE_PARENT(rbe))))
                rbe = OSSL_RBE_PARENT(rbe);
            rbe = OSSL_RBE_PARENT(rbe);
        }
    }

    return rbe == NULL ? NULL : rbt_e2n(t, rbe);
}

void *
ossl_rbt_root(const struct ossl_rbt_type *t, struct ossl_rbt_tree *rbt)
{
    struct ossl_rbt_entry *rbe = OSSL_RBH_ROOT(rbt);

    return rbe == NULL ? rbe : rbt_e2n(t, rbe);
}

void *
ossl_rbt_min(const struct ossl_rbt_type *t, struct ossl_rbt_tree *rbt)
{
    struct ossl_rbt_entry *rbe = OSSL_RBH_ROOT(rbt);
    struct ossl_rbt_entry *parent = NULL;

    while (rbe != NULL) {
        parent = rbe;
        rbe = OSSL_RBE_LEFT(rbe);
    }

    return parent == NULL ? NULL : rbt_e2n(t, parent);
}

void *
ossl_rbt_max(const struct ossl_rbt_type *t, struct ossl_rbt_tree *rbt)
{
    struct ossl_rbt_entry *rbe = OSSL_RBH_ROOT(rbt);
    struct ossl_rbt_entry *parent = NULL;

    while (rbe != NULL) {
        parent = rbe;
        rbe = OSSL_RBE_RIGHT(rbe);
    }

    return parent == NULL ? NULL : rbt_e2n(t, parent);
}

void *
ossl_rbt_left(const struct ossl_rbt_type *t, void *node)
{
    struct ossl_rbt_entry *rbe = rbt_n2e(t, node);
    rbe = OSSL_RBE_LEFT(rbe);
    return rbe == NULL ? NULL : rbt_e2n(t, rbe);
}

void *
ossl_rbt_right(const struct ossl_rbt_type *t, void *node)
{
    struct ossl_rbt_entry *rbe = rbt_n2e(t, node);
    rbe = OSSL_RBE_RIGHT(rbe);
    return rbe == NULL ? NULL : rbt_e2n(t, rbe);
}

void *
ossl_rbt_parent(const struct ossl_rbt_type *t, void *node)
{
    struct ossl_rbt_entry *rbe = rbt_n2e(t, node);
    rbe = OSSL_RBE_PARENT(rbe);
    return rbe == NULL ? NULL : rbt_e2n(t, rbe);
}

void ossl_rbt_set_left(const struct ossl_rbt_type *t, void *node, void *left)
{
    struct ossl_rbt_entry *rbe = rbt_n2e(t, node);
    struct ossl_rbt_entry *rbl = (left == NULL) ? NULL : rbt_n2e(t, left);

    OSSL_RBE_LEFT(rbe) = rbl;
}

void ossl_rbt_set_right(const struct ossl_rbt_type *t, void *node, void *right)
{
    struct ossl_rbt_entry *rbe = rbt_n2e(t, node);
    struct ossl_rbt_entry *rbr = (right == NULL) ? NULL : rbt_n2e(t, right);

    OSSL_RBE_RIGHT(rbe) = rbr;
}

void ossl_rbt_set_parent(const struct ossl_rbt_type *t, void *node, void *parent)
{
    struct ossl_rbt_entry *rbe = rbt_n2e(t, node);
    struct ossl_rbt_entry *rbp = (parent == NULL) ? NULL : rbt_n2e(t, parent);

    OSSL_RBE_PARENT(rbe) = rbp;
}

void ossl_rbt_init_rbe(const struct ossl_rbt_type *t, void *node)
{
    struct ossl_rbt_entry *rbe = rbt_n2e(t, node);

    OSSL_RBE_PARENT(rbe) = NULL;
    OSSL_RBE_LEFT(rbe) = NULL;
    OSSL_RBE_RIGHT(rbe) = NULL;
}
