/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2013 Tatsuhiro Tsujikawa
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
#include "nghttp2_hd_huffman.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "nghttp2_hd.h"

/*
 * Encodes huffman code |sym| into |*dest_ptr|, whose least |rembits|
 * bits are not filled yet.  The |rembits| must be in range [1, 8],
 * inclusive.  At the end of the process, the |*dest_ptr| is updated
 * and points where next output should be placed. The number of
 * unfilled bits in the pointed location is returned.
 */
static ssize_t huff_encode_sym(nghttp2_bufs *bufs, size_t *avail_ptr,
                               size_t rembits, const nghttp2_huff_sym *sym) {
  int rv;
  size_t nbits = sym->nbits;
  uint32_t code = sym->code;

  /* We assume that sym->nbits <= 32 */
  if (rembits > nbits) {
    nghttp2_bufs_fast_orb_hold(bufs, (uint8_t)(code << (rembits - nbits)));
    return (ssize_t)(rembits - nbits);
  }

  if (rembits == nbits) {
    nghttp2_bufs_fast_orb(bufs, (uint8_t)code);
    --*avail_ptr;
    return 8;
  }

  nghttp2_bufs_fast_orb(bufs, (uint8_t)(code >> (nbits - rembits)));
  --*avail_ptr;

  nbits -= rembits;
  if (nbits & 0x7) {
    /* align code to MSB byte boundary */
    code <<= 8 - (nbits & 0x7);
  }

  if (*avail_ptr < (nbits + 7) / 8) {
    /* slow path */
    if (nbits > 24) {
      rv = nghttp2_bufs_addb(bufs, (uint8_t)(code >> 24));
      if (rv != 0) {
        return rv;
      }
      nbits -= 8;
    }
    if (nbits > 16) {
      rv = nghttp2_bufs_addb(bufs, (uint8_t)(code >> 16));
      if (rv != 0) {
        return rv;
      }
      nbits -= 8;
    }
    if (nbits > 8) {
      rv = nghttp2_bufs_addb(bufs, (uint8_t)(code >> 8));
      if (rv != 0) {
        return rv;
      }
      nbits -= 8;
    }
    if (nbits == 8) {
      rv = nghttp2_bufs_addb(bufs, (uint8_t)code);
      if (rv != 0) {
        return rv;
      }
      *avail_ptr = nghttp2_bufs_cur_avail(bufs);
      return 8;
    }

    rv = nghttp2_bufs_addb_hold(bufs, (uint8_t)code);
    if (rv != 0) {
      return rv;
    }
    *avail_ptr = nghttp2_bufs_cur_avail(bufs);
    return (ssize_t)(8 - nbits);
  }

  /* fast path, since most code is less than 8 */
  if (nbits < 8) {
    nghttp2_bufs_fast_addb_hold(bufs, (uint8_t)code);
    *avail_ptr = nghttp2_bufs_cur_avail(bufs);
    return (ssize_t)(8 - nbits);
  }

  /* handle longer code path */
  if (nbits > 24) {
    nghttp2_bufs_fast_addb(bufs, (uint8_t)(code >> 24));
    nbits -= 8;
  }

  if (nbits > 16) {
    nghttp2_bufs_fast_addb(bufs, (uint8_t)(code >> 16));
    nbits -= 8;
  }

  if (nbits > 8) {
    nghttp2_bufs_fast_addb(bufs, (uint8_t)(code >> 8));
    nbits -= 8;
  }

  if (nbits == 8) {
    nghttp2_bufs_fast_addb(bufs, (uint8_t)code);
    *avail_ptr = nghttp2_bufs_cur_avail(bufs);
    return 8;
  }

  nghttp2_bufs_fast_addb_hold(bufs, (uint8_t)code);
  *avail_ptr = nghttp2_bufs_cur_avail(bufs);
  return (ssize_t)(8 - nbits);
}

size_t nghttp2_hd_huff_encode_count(const uint8_t *src, size_t len) {
  size_t i;
  size_t nbits = 0;

  for (i = 0; i < len; ++i) {
    nbits += huff_sym_table[src[i]].nbits;
  }
  /* pad the prefix of EOS (256) */
  return (nbits + 7) / 8;
}

int nghttp2_hd_huff_encode(nghttp2_bufs *bufs, const uint8_t *src,
                           size_t srclen) {
  int rv;
  ssize_t rembits = 8;
  size_t i;
  size_t avail;

  avail = nghttp2_bufs_cur_avail(bufs);

  for (i = 0; i < srclen; ++i) {
    const nghttp2_huff_sym *sym = &huff_sym_table[src[i]];
    if (rembits == 8) {
      if (avail) {
        nghttp2_bufs_fast_addb_hold(bufs, 0);
      } else {
        rv = nghttp2_bufs_addb_hold(bufs, 0);
        if (rv != 0) {
          return rv;
        }
        avail = nghttp2_bufs_cur_avail(bufs);
      }
    }
    rembits = huff_encode_sym(bufs, &avail, (size_t)rembits, sym);
    if (rembits < 0) {
      return (int)rembits;
    }
  }
  /* 256 is special terminal symbol, pad with its prefix */
  if (rembits < 8) {
    /* if rembits < 8, we should have at least 1 buffer space
       available */
    const nghttp2_huff_sym *sym = &huff_sym_table[256];
    assert(avail);
    /* Caution we no longer adjust avail here */
    nghttp2_bufs_fast_orb(
        bufs, (uint8_t)(sym->code >> (sym->nbits - (size_t)rembits)));
  }

  return 0;
}

void nghttp2_hd_huff_decode_context_init(nghttp2_hd_huff_decode_context *ctx) {
  ctx->state = 0;
  ctx->accept = 1;
}

ssize_t nghttp2_hd_huff_decode(nghttp2_hd_huff_decode_context *ctx,
                               nghttp2_buf *buf, const uint8_t *src,
                               size_t srclen, int final) {
  size_t i;

  /* We use the decoding algorithm described in
     http://graphics.ics.uci.edu/pub/Prefix.pdf */
  for (i = 0; i < srclen; ++i) {
    const nghttp2_huff_decode *t;

    t = &huff_decode_table[ctx->state][src[i] >> 4];
    if (t->flags & NGHTTP2_HUFF_FAIL) {
      return NGHTTP2_ERR_HEADER_COMP;
    }
    if (t->flags & NGHTTP2_HUFF_SYM) {
      *buf->last++ = t->sym;
    }

    t = &huff_decode_table[t->state][src[i] & 0xf];
    if (t->flags & NGHTTP2_HUFF_FAIL) {
      return NGHTTP2_ERR_HEADER_COMP;
    }
    if (t->flags & NGHTTP2_HUFF_SYM) {
      *buf->last++ = t->sym;
    }

    ctx->state = t->state;
    ctx->accept = (t->flags & NGHTTP2_HUFF_ACCEPTED) != 0;
  }
  if (final && !ctx->accept) {
    return NGHTTP2_ERR_HEADER_COMP;
  }
  return (ssize_t)i;
}
