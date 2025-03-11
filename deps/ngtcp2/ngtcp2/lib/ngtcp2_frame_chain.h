/*
 * ngtcp2
 *
 * Copyright (c) 2023 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef NGTCP2_FRAME_CHAIN_H
#define NGTCP2_FRAME_CHAIN_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* defined(HAVE_CONFIG_H) */

#include <ngtcp2/ngtcp2.h>

#include "ngtcp2_pkt.h"
#include "ngtcp2_objalloc.h"

/* NGTCP2_FRAME_CHAIN_BINDER_FLAG_NONE indicates that no flag is
   set. */
#define NGTCP2_FRAME_CHAIN_BINDER_FLAG_NONE 0x00u
/* NGTCP2_FRAME_CHAIN_BINDER_FLAG_ACK indicates that an information
   which a frame carries has been acknowledged. */
#define NGTCP2_FRAME_CHAIN_BINDER_FLAG_ACK 0x01u

/*
 * ngtcp2_frame_chain_binder binds 2 or more of ngtcp2_frame_chain to
 * share the acknowledgement state.  In general, all
 * ngtcp2_frame_chains bound to the same binder must have the same
 * information.
 */
typedef struct ngtcp2_frame_chain_binder {
  size_t refcount;
  /* flags is bitwise OR of zero or more of
     NGTCP2_FRAME_CHAIN_BINDER_FLAG_*. */
  uint32_t flags;
} ngtcp2_frame_chain_binder;

int ngtcp2_frame_chain_binder_new(ngtcp2_frame_chain_binder **pbinder,
                                  const ngtcp2_mem *mem);

typedef struct ngtcp2_frame_chain ngtcp2_frame_chain;

/*
 * ngtcp2_frame_chain chains frames in a single packet.
 */
struct ngtcp2_frame_chain {
  union {
    struct {
      ngtcp2_frame_chain *next;
      ngtcp2_frame_chain_binder *binder;
      ngtcp2_frame fr;
    };

    ngtcp2_opl_entry oplent;
  };
};

ngtcp2_objalloc_decl(frame_chain, ngtcp2_frame_chain, oplent)

/*
 * ngtcp2_bind_frame_chains binds two frame chains |a| and |b| using
 * new or existing ngtcp2_frame_chain_binder.  |a| might have non-NULL
 * a->binder.  |b| must not have non-NULL b->binder.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 */
int ngtcp2_bind_frame_chains(ngtcp2_frame_chain *a, ngtcp2_frame_chain *b,
                             const ngtcp2_mem *mem);

/* NGTCP2_MAX_STREAM_DATACNT is the maximum number of ngtcp2_vec that
   a ngtcp2_stream can include. */
#define NGTCP2_MAX_STREAM_DATACNT 256

/*
 * ngtcp2_frame_chain_objalloc_new allocates ngtcp2_frame_chain using
 * |objalloc|.
 */
int ngtcp2_frame_chain_objalloc_new(ngtcp2_frame_chain **pfrc,
                                    ngtcp2_objalloc *objalloc);

/*
 * ngtcp2_frame_chain_extralen_new allocates ngtcp2_frame_chain
 * followed by |extralen| bytes.
 */
int ngtcp2_frame_chain_extralen_new(ngtcp2_frame_chain **pfrc, size_t extralen,
                                    const ngtcp2_mem *mem);

/* NGTCP2_FRAME_CHAIN_STREAM_AVAIL is the number of additional bytes
   available after ngtcp2_stream when it is embedded in
   ngtcp2_frame. */
#define NGTCP2_FRAME_CHAIN_STREAM_AVAIL                                        \
  (sizeof(ngtcp2_frame) - sizeof(ngtcp2_stream))

/* NGTCP2_FRAME_CHAIN_STREAM_DATACNT_THRES is the number of datacnt
   that changes allocation method.  If datacnt is more than this
   value, ngtcp2_frame_chain is allocated without ngtcp2_objalloc.
   Otherwise, it is allocated using ngtcp2_objalloc.  */
#define NGTCP2_FRAME_CHAIN_STREAM_DATACNT_THRES                                \
  (NGTCP2_FRAME_CHAIN_STREAM_AVAIL / sizeof(ngtcp2_vec) + 1)

/* NGTCP2_FRAME_CHAIN_NEW_TOKEN_THRES is the length of a token that
   changes allocation method.  If the length is more than this value,
   ngtcp2_frame_chain is allocated without ngtcp2_objalloc.
   Otherwise, it is allocated using ngtcp2_objalloc. */
#define NGTCP2_FRAME_CHAIN_NEW_TOKEN_THRES                                     \
  (sizeof(ngtcp2_frame) - sizeof(ngtcp2_new_token))

/*
 * ngtcp2_frame_chain_stream_datacnt_objalloc_new allocates enough
 * data to store additional |datacnt| - 1 ngtcp2_vec object after
 * ngtcp2_stream object.  If no additional space is required, in other
 * words, |datacnt| <= NGTCP2_FRAME_CHAIN_STREAM_DATACNT_THRES,
 * ngtcp2_frame_chain_objalloc_new is called internally.  Otherwise,
 * ngtcp2_frame_chain_extralen_new is used and objalloc is not used.
 * Therefore, it is important to call ngtcp2_frame_chain_objalloc_del
 * without changing datacnt field.
 */
int ngtcp2_frame_chain_stream_datacnt_objalloc_new(ngtcp2_frame_chain **pfrc,
                                                   size_t datacnt,
                                                   ngtcp2_objalloc *objalloc,
                                                   const ngtcp2_mem *mem);

/*
 * ngtcp2_frame_chain_new_token_objalloc_new allocates enough space to
 * store the given token.  If |tokenlen| <=
 * NGTCP2_FRAME_CHAIN_NEW_TOKEN_THRES, ngtcp2_frame_chain_objalloc_new
 * is called internally.  Otherwise, ngtcp2_frame_chain_extralen_new
 * is used, and objalloc is not used.
 */
int ngtcp2_frame_chain_new_token_objalloc_new(ngtcp2_frame_chain **pfrc,
                                              const uint8_t *token,
                                              size_t tokenlen,
                                              ngtcp2_objalloc *objalloc,
                                              const ngtcp2_mem *mem);

/*
 * ngtcp2_frame_chain_del deallocates |frc|.  It also deallocates the
 * memory pointed by |frc|.
 */
void ngtcp2_frame_chain_del(ngtcp2_frame_chain *frc, const ngtcp2_mem *mem);

/*
 * ngtcp2_frame_chain_objalloc_del adds |frc| to |objalloc| for reuse.
 * It might just delete |frc| depending on the frame type and the size
 * of |frc|.
 */
void ngtcp2_frame_chain_objalloc_del(ngtcp2_frame_chain *frc,
                                     ngtcp2_objalloc *objalloc,
                                     const ngtcp2_mem *mem);

/*
 * ngtcp2_frame_chain_init initializes |frc|.
 */
void ngtcp2_frame_chain_init(ngtcp2_frame_chain *frc);

/*
 * ngtcp2_frame_chain_list_objalloc_del adds all ngtcp2_frame_chain
 * linked from |frc| to |objalloc| for reuse.  Depending on the frame type
 * and its size, ngtcp2_frame_chain might be deleted instead.
 */
void ngtcp2_frame_chain_list_objalloc_del(ngtcp2_frame_chain *frc,
                                          ngtcp2_objalloc *objalloc,
                                          const ngtcp2_mem *mem);

#endif /* !defined(NGTCP2_FRAME_CHAIN_H) */
