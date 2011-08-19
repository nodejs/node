/* pcy_node.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2004.
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

#include <openssl/asn1.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "pcy_int.h"

static int node_cmp(const X509_POLICY_NODE * const *a,
			const X509_POLICY_NODE * const *b)
	{
	return OBJ_cmp((*a)->data->valid_policy, (*b)->data->valid_policy);
	}

STACK_OF(X509_POLICY_NODE) *policy_node_cmp_new(void)
	{
	return sk_X509_POLICY_NODE_new(node_cmp);
	}

X509_POLICY_NODE *tree_find_sk(STACK_OF(X509_POLICY_NODE) *nodes,
					const ASN1_OBJECT *id)
	{
	X509_POLICY_DATA n;
	X509_POLICY_NODE l;
	int idx;

	n.valid_policy = (ASN1_OBJECT *)id;
	l.data = &n;

	idx = sk_X509_POLICY_NODE_find(nodes, &l);
	if (idx == -1)
		return NULL;

	return sk_X509_POLICY_NODE_value(nodes, idx);

	}

X509_POLICY_NODE *level_find_node(const X509_POLICY_LEVEL *level,
					const ASN1_OBJECT *id)
	{
	return tree_find_sk(level->nodes, id);
	}

X509_POLICY_NODE *level_add_node(X509_POLICY_LEVEL *level,
			X509_POLICY_DATA *data,
			X509_POLICY_NODE *parent,
			X509_POLICY_TREE *tree)
	{
	X509_POLICY_NODE *node;
	node = OPENSSL_malloc(sizeof(X509_POLICY_NODE));
	if (!node)
		return NULL;
	node->data = data;
	node->parent = parent;
	node->nchild = 0;
	if (level)
		{
		if (OBJ_obj2nid(data->valid_policy) == NID_any_policy)
			{
			if (level->anyPolicy)
				goto node_error;
			level->anyPolicy = node;
			}
		else
			{

			if (!level->nodes)
				level->nodes = policy_node_cmp_new();
			if (!level->nodes)
				goto node_error;
			if (!sk_X509_POLICY_NODE_push(level->nodes, node))
				goto node_error;
			}
		}

	if (tree)
		{
		if (!tree->extra_data)
			 tree->extra_data = sk_X509_POLICY_DATA_new_null();
		if (!tree->extra_data)
			goto node_error;
		if (!sk_X509_POLICY_DATA_push(tree->extra_data, data))
			goto node_error;
		}

	if (parent)
		parent->nchild++;

	return node;

	node_error:
	policy_node_free(node);
	return 0;

	}

void policy_node_free(X509_POLICY_NODE *node)
	{
	OPENSSL_free(node);
	}


