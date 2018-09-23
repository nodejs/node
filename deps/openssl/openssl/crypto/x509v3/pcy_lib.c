/* pcy_lib.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2004.
 */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved.
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

#include "cryptlib.h"
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "pcy_int.h"

/* accessor functions */

/* X509_POLICY_TREE stuff */

int X509_policy_tree_level_count(const X509_POLICY_TREE *tree)
{
    if (!tree)
        return 0;
    return tree->nlevel;
}

X509_POLICY_LEVEL *X509_policy_tree_get0_level(const X509_POLICY_TREE *tree,
                                               int i)
{
    if (!tree || (i < 0) || (i >= tree->nlevel))
        return NULL;
    return tree->levels + i;
}

STACK_OF(X509_POLICY_NODE) *X509_policy_tree_get0_policies(const
                                                           X509_POLICY_TREE
                                                           *tree)
{
    if (!tree)
        return NULL;
    return tree->auth_policies;
}

STACK_OF(X509_POLICY_NODE) *X509_policy_tree_get0_user_policies(const
                                                                X509_POLICY_TREE
                                                                *tree)
{
    if (!tree)
        return NULL;
    if (tree->flags & POLICY_FLAG_ANY_POLICY)
        return tree->auth_policies;
    else
        return tree->user_policies;
}

/* X509_POLICY_LEVEL stuff */

int X509_policy_level_node_count(X509_POLICY_LEVEL *level)
{
    int n;
    if (!level)
        return 0;
    if (level->anyPolicy)
        n = 1;
    else
        n = 0;
    if (level->nodes)
        n += sk_X509_POLICY_NODE_num(level->nodes);
    return n;
}

X509_POLICY_NODE *X509_policy_level_get0_node(X509_POLICY_LEVEL *level, int i)
{
    if (!level)
        return NULL;
    if (level->anyPolicy) {
        if (i == 0)
            return level->anyPolicy;
        i--;
    }
    return sk_X509_POLICY_NODE_value(level->nodes, i);
}

/* X509_POLICY_NODE stuff */

const ASN1_OBJECT *X509_policy_node_get0_policy(const X509_POLICY_NODE *node)
{
    if (!node)
        return NULL;
    return node->data->valid_policy;
}

#if 0
int X509_policy_node_get_critical(const X509_POLICY_NODE *node)
{
    if (node_critical(node))
        return 1;
    return 0;
}
#endif

STACK_OF(POLICYQUALINFO) *X509_policy_node_get0_qualifiers(const
                                                           X509_POLICY_NODE
                                                           *node)
{
    if (!node)
        return NULL;
    return node->data->qualifier_set;
}

const X509_POLICY_NODE *X509_policy_node_get0_parent(const X509_POLICY_NODE
                                                     *node)
{
    if (!node)
        return NULL;
    return node->parent;
}
