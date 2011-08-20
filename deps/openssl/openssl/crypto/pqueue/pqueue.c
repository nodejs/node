/* crypto/pqueue/pqueue.c */
/* 
 * DTLS implementation written by Nagendra Modadugu
 * (nagendra@cs.stanford.edu) for the OpenSSL project 2005.  
 */
/* ====================================================================
 * Copyright (c) 1999-2005 The OpenSSL Project.  All rights reserved.
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
 *    openssl-core@OpenSSL.org.
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
#include <openssl/bn.h>
#include "pqueue.h"

typedef struct _pqueue
	{
	pitem *items;
	int count;
	} pqueue_s;

pitem *
pitem_new(PQ_64BIT priority, void *data)
	{
	pitem *item = (pitem *) OPENSSL_malloc(sizeof(pitem));
	if (item == NULL) return NULL;

	pq_64bit_init(&(item->priority));
	pq_64bit_assign(&item->priority, &priority);

	item->data = data;
	item->next = NULL;

	return item;
	}

void
pitem_free(pitem *item)
	{
	if (item == NULL) return;

	pq_64bit_free(&(item->priority));
	OPENSSL_free(item);
	}

pqueue_s *
pqueue_new()
	{
	pqueue_s *pq = (pqueue_s *) OPENSSL_malloc(sizeof(pqueue_s));
	if (pq == NULL) return NULL;

	memset(pq, 0x00, sizeof(pqueue_s));
	return pq;
	}

void
pqueue_free(pqueue_s *pq)
	{
	if (pq == NULL) return;

	OPENSSL_free(pq);
	}

pitem *
pqueue_insert(pqueue_s *pq, pitem *item)
	{
	pitem *curr, *next;

	if (pq->items == NULL)
		{
		pq->items = item;
		return item;
		}

	for(curr = NULL, next = pq->items; 
		next != NULL;
		curr = next, next = next->next)
		{
		if (pq_64bit_gt(&(next->priority), &(item->priority)))
			{
			item->next = next;

			if (curr == NULL) 
				pq->items = item;
			else  
				curr->next = item;

			return item;
			}
		/* duplicates not allowed */
		if (pq_64bit_eq(&(item->priority), &(next->priority)))
			return NULL;
		}

	item->next = NULL;
	curr->next = item;

	return item;
	}

pitem *
pqueue_peek(pqueue_s *pq)
	{
	return pq->items;
	}

pitem *
pqueue_pop(pqueue_s *pq)
	{
	pitem *item = pq->items;

	if (pq->items != NULL)
		pq->items = pq->items->next;

	return item;
	}

pitem *
pqueue_find(pqueue_s *pq, PQ_64BIT priority)
	{
	pitem *next;
	pitem *found = NULL;

	if ( pq->items == NULL)
		return NULL;

	for ( next = pq->items; next->next != NULL; next = next->next)
		{
		if ( pq_64bit_eq(&(next->priority), &priority))
			{
			found = next;
			break;
			}
		}
	
	/* check the one last node */
	if ( pq_64bit_eq(&(next->priority), &priority))
		found = next;

	if ( ! found)
		return NULL;

	return found;
	}

#if PQ_64BIT_IS_INTEGER
void
pqueue_print(pqueue_s *pq)
	{
	pitem *item = pq->items;

	while(item != NULL)
		{
		printf("item\t" PQ_64BIT_PRINT "\n", item->priority);
		item = item->next;
		}
	}
#endif

pitem *
pqueue_iterator(pqueue_s *pq)
	{
	return pqueue_peek(pq);
	}

pitem *
pqueue_next(pitem **item)
	{
	pitem *ret;

	if ( item == NULL || *item == NULL)
		return NULL;


	/* *item != NULL */
	ret = *item;
	*item = (*item)->next;

	return ret;
	}

int
pqueue_size(pqueue_s *pq)
{
	pitem *item = pq->items;
	int count = 0;
	
	while(item != NULL)
	{
		count++;
		item = item->next;
	}
	return count;
}
