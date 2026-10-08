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
#ifndef _OSSL_INTERNAL_RBTREE_H_
#define _OSSL_INTERNAL_RBTREE_H_

#include "internal/e_os.h"

/*
 * List of changes against upstream version:
 *   augmentation mechanism is removed in OpenSSL as there is no demand for it
 *
 *   prefix changed from rb/rbt to ossl_rbt
 *
 *   debug version of OSSL_RBT_REMOVE() sets parent, left, right members
 *   to NULL
 *
 *   cstyle is changed to match OpenSSL.
 */
struct ossl_rbt_type {
    int (*t_compare)(const void *, const void *);
    uintptr_t t_offset; /* offset of ossl_rbt_entry in type */
};

struct ossl_rbt_tree {
    struct ossl_rbt_entry *rb_root;
};

struct ossl_rbt_entry {
    struct ossl_rbt_entry *rb_parent;
    struct ossl_rbt_entry *rb_left;
    struct ossl_rbt_entry *rb_right;
    unsigned int rb_color;
};

#define OSSL_RBT_HEAD(_name, _type)    \
    struct _name {                     \
        struct ossl_rbt_tree rbh_root; \
    }

#define OSSL_RBT_ENTRY(_type) struct ossl_rbt_entry

static ossl_inline void
ossl_rbt_init(struct ossl_rbt_tree *rb)
{
    rb->rb_root = NULL;
}

static ossl_inline int
ossl_rbt_empty(struct ossl_rbt_tree *rb)
{
    return rb->rb_root == NULL;
}

void *ossl_rbt_insert(const struct ossl_rbt_type *, struct ossl_rbt_tree *, void *);
void *ossl_rbt_remove(const struct ossl_rbt_type *, struct ossl_rbt_tree *, void *);
void *ossl_rbt_find(const struct ossl_rbt_type *, struct ossl_rbt_tree *, const void *);
void *ossl_rbt_nfind(const struct ossl_rbt_type *, struct ossl_rbt_tree *, const void *);
void *ossl_rbt_root(const struct ossl_rbt_type *, struct ossl_rbt_tree *);
void *ossl_rbt_min(const struct ossl_rbt_type *, struct ossl_rbt_tree *);
void *ossl_rbt_max(const struct ossl_rbt_type *, struct ossl_rbt_tree *);
void *ossl_rbt_next(const struct ossl_rbt_type *, void *);
void *ossl_rbt_prev(const struct ossl_rbt_type *, void *);
void *ossl_rbt_left(const struct ossl_rbt_type *, void *);
void *ossl_rbt_right(const struct ossl_rbt_type *, void *);
void *ossl_rbt_parent(const struct ossl_rbt_type *, void *);
void ossl_rbt_set_left(const struct ossl_rbt_type *, void *, void *);
void ossl_rbt_set_right(const struct ossl_rbt_type *, void *, void *);
void ossl_rbt_set_parent(const struct ossl_rbt_type *, void *, void *);
void ossl_rbt_init_rbe(const struct ossl_rbt_type *, void *);

#define OSSL_RBT_INITIALIZER(_head) \
    {                               \
        {                           \
            NULL                    \
        }                           \
    }

#define OSSL_RBT_PROTOTYPE(_name, _type, _field, _cmp)                       \
    extern const struct ossl_rbt_type *const _name##_OSSL_RBT_TYPE;          \
                                                                             \
    ossl_unused static ossl_inline void                                      \
    _name##_OSSL_RBT_INIT(struct _name *head)                                \
    {                                                                        \
        ossl_rbt_init(&head->rbh_root);                                      \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline struct _type *                            \
    _name##_OSSL_RBT_INSERT(struct _name *head, struct _type *elm)           \
    {                                                                        \
        return ossl_rbt_insert(_name##_OSSL_RBT_TYPE, &head->rbh_root, elm); \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline struct _type *                            \
    _name##_OSSL_RBT_REMOVE(struct _name *head, struct _type *elm)           \
    {                                                                        \
        return ossl_rbt_remove(_name##_OSSL_RBT_TYPE, &head->rbh_root, elm); \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline struct _type *                            \
    _name##_OSSL_RBT_FIND(struct _name *head, const struct _type *key)       \
    {                                                                        \
        return ossl_rbt_find(_name##_OSSL_RBT_TYPE, &head->rbh_root, key);   \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline struct _type *                            \
    _name##_OSSL_RBT_NFIND(struct _name *head, const struct _type *key)      \
    {                                                                        \
        return ossl_rbt_nfind(_name##_OSSL_RBT_TYPE, &head->rbh_root, key);  \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline struct _type *                            \
    _name##_OSSL_RBT_ROOT(struct _name *head)                                \
    {                                                                        \
        return ossl_rbt_root(_name##_OSSL_RBT_TYPE, &head->rbh_root);        \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline int                                       \
    _name##_OSSL_RBT_EMPTY(struct _name *head)                               \
    {                                                                        \
        return ossl_rbt_empty(&head->rbh_root);                              \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline struct _type *                            \
    _name##_OSSL_RBT_MIN(struct _name *head)                                 \
    {                                                                        \
        return ossl_rbt_min(_name##_OSSL_RBT_TYPE, &head->rbh_root);         \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline struct _type *                            \
    _name##_OSSL_RBT_MAX(struct _name *head)                                 \
    {                                                                        \
        return ossl_rbt_max(_name##_OSSL_RBT_TYPE, &head->rbh_root);         \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline struct _type *                            \
    _name##_OSSL_RBT_NEXT(struct _type *elm)                                 \
    {                                                                        \
        return ossl_rbt_next(_name##_OSSL_RBT_TYPE, elm);                    \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline struct _type *                            \
    _name##_OSSL_RBT_PREV(struct _type *elm)                                 \
    {                                                                        \
        return ossl_rbt_prev(_name##_OSSL_RBT_TYPE, elm);                    \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline struct _type *                            \
    _name##_OSSL_RBT_LEFT(struct _type *elm)                                 \
    {                                                                        \
        return ossl_rbt_left(_name##_OSSL_RBT_TYPE, elm);                    \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline struct _type *                            \
    _name##_OSSL_RBT_RIGHT(struct _type *elm)                                \
    {                                                                        \
        return ossl_rbt_right(_name##_OSSL_RBT_TYPE, elm);                   \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline struct _type *                            \
    _name##_OSSL_RBT_PARENT(struct _type *elm)                               \
    {                                                                        \
        return ossl_rbt_parent(_name##_OSSL_RBT_TYPE, elm);                  \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline void                                      \
    _name##_OSSL_RBT_SET_LEFT(struct _type *elm, struct _type *left)         \
    {                                                                        \
        ossl_rbt_set_left(_name##_OSSL_RBT_TYPE, elm, left);                 \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline void                                      \
    _name##_OSSL_RBT_SET_RIGHT(struct _type *elm, struct _type *right)       \
    {                                                                        \
        ossl_rbt_set_right(_name##_OSSL_RBT_TYPE, elm, right);               \
    }                                                                        \
                                                                             \
    ossl_unused static ossl_inline void                                      \
    _name##_OSSL_RBT_SET_PARENT(struct _type *elm, struct _type *parent)     \
    {                                                                        \
        ossl_rbt_set_parent(_name##_OSSL_RBT_TYPE, elm, parent);             \
    }                                                                        \
    ossl_unused static ossl_inline void                                      \
    _name##_OSSL_RBT_INIT_RBE(struct _type *elm)                             \
    {                                                                        \
        ossl_rbt_init_rbe(_name##_OSSL_RBT_TYPE, elm);                       \
    }

#define OSSL_RBT_GENERATE_INTERNAL(_name, _type, _field, _cmp)   \
    static int                                                   \
    _name##_OSSL_RBT_COMPARE(const void *lptr, const void *rptr) \
    {                                                            \
        const struct _type *l = lptr, *r = rptr;                 \
        return _cmp(l, r);                                       \
    }                                                            \
    static const struct ossl_rbt_type _name##_OSSL_RBT_INFO = {  \
        _name##_OSSL_RBT_COMPARE,                                \
        offsetof(struct _type, _field),                          \
    };                                                           \
    const struct ossl_rbt_type *const _name##_OSSL_RBT_TYPE = &_name##_OSSL_RBT_INFO

#define OSSL_RBT_GENERATE(_name, _type, _field, _cmp) \
    OSSL_RBT_GENERATE_INTERNAL(_name, _type, _field, _cmp)

#define OSSL_RBT_INIT(_name, _head) _name##_OSSL_RBT_INIT(_head)
#define OSSL_RBT_INSERT(_name, _head, _elm) _name##_OSSL_RBT_INSERT(_head, _elm)
#define OSSL_RBT_REMOVE(_name, _head, _elm) _name##_OSSL_RBT_REMOVE(_head, _elm)
#define OSSL_RBT_FIND(_name, _head, _key) _name##_OSSL_RBT_FIND(_head, _key)
#define OSSL_RBT_NFIND(_name, _head, _key) _name##_OSSL_RBT_NFIND(_head, _key)
#define OSSL_RBT_ROOT(_name, _head) _name##_OSSL_RBT_ROOT(_head)
#define OSSL_RBT_EMPTY(_name, _head) _name##_OSSL_RBT_EMPTY(_head)
#define OSSL_RBT_MIN(_name, _head) _name##_OSSL_RBT_MIN(_head)
#define OSSL_RBT_MAX(_name, _head) _name##_OSSL_RBT_MAX(_head)
#define OSSL_RBT_NEXT(_name, _elm) _name##_OSSL_RBT_NEXT(_elm)
#define OSSL_RBT_PREV(_name, _elm) _name##_OSSL_RBT_PREV(_elm)
#define OSSL_RBT_LEFT(_name, _elm) _name##_OSSL_RBT_LEFT(_elm)
#define OSSL_RBT_RIGHT(_name, _elm) _name##_OSSL_RBT_RIGHT(_elm)
#define OSSL_RBT_PARENT(_name, _elm) _name##_OSSL_RBT_PARENT(_elm)
#define OSSL_RBT_SET_LEFT(_name, _elm, _l) _name##_OSSL_RBT_SET_LEFT(_elm, _l)
#define OSSL_RBT_SET_RIGHT(_name, _elm, _r) _name##_OSSL_RBT_SET_RIGHT(_elm, _r)
#define OSSL_RBT_SET_PARENT(_name, _elm, _p) _name##_OSSL_RBT_SET_PARENT(_elm, _p)
#ifndef NDEBUG
#define OSSL_RBT_INIT_RBE(_name, _elm) _name##_OSSL_RBT_INIT_RBE(_elm)
#else
#define OSSL_RBT_INIT_RBE(_name, _elm) (void)(0)
#endif

#define OSSL_RBT_FOREACH(_e, _name, _head)    \
    for ((_e) = OSSL_RBT_MIN(_name, (_head)); \
        (_e) != NULL;                         \
        (_e) = OSSL_RBT_NEXT(_name, (_e)))

#define OSSL_RBT_FOREACH_SAFE(_e, _name, _head, _n)             \
    for ((_e) = OSSL_RBT_MIN(_name, (_head));                   \
        (_e) != NULL && ((_n) = OSSL_RBT_NEXT(_name, (_e)), 1); \
        (_e) = (_n))

#define OSSL_RBT_FOREACH_REVERSE(_e, _name, _head) \
    for ((_e) = OSSL_RBT_MAX(_name, (_head));      \
        (_e) != NULL;                              \
        (_e) = OSSL_RBT_PREV(_name, (_e)))

#define OSSL_RBT_FOREACH_REVERSE_SAFE(_e, _name, _head, _n)     \
    for ((_e) = OSSL_RBT_MAX(_name, (_head));                   \
        (_e) != NULL && ((_n) = OSSL_RBT_PREV(_name, (_e)), 1); \
        (_e) = (_n))

#endif /* _OSSL_INTERNAL_RBTREE_H_ */
