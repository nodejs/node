/* inffas8664.c is a hand tuned assembler version of inffast.c - fast decoding
 * version for AMD64 on Windows using Microsoft C compiler
 *
 * Copyright (C) 1995-2003 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Copyright (C) 2003 Chris Anderson <christop@charm.net>
 * Please use the copyright conditions above.
 *
 * 2005 - Adaptation to Microsoft C Compiler for AMD64 by Gilles Vollant
 *
 * inffas8664.c call function inffas8664fnc in inffasx64.asm
 *  inffasx64.asm is automatically convert from AMD64 portion of inffas86.c
 *
 * Dec-29-2003 -- I added AMD64 inflate asm support.  This version is also
 * slightly quicker on x86 systems because, instead of using rep movsb to copy
 * data, it uses rep movsw, which moves data in 2-byte chunks instead of single
 * bytes.  I've tested the AMD64 code on a Fedora Core 1 + the x86_64 updates
 * from http://fedora.linux.duke.edu/fc1_x86_64
 * which is running on an Athlon 64 3000+ / Gigabyte GA-K8VT800M system with
 * 1GB ram.  The 64-bit version is about 4% faster than the 32-bit version,
 * when decompressing mozilla-source-1.3.tar.gz.
 *
 * Mar-13-2003 -- Most of this is derived from inffast.S which is derived from
 * the gcc -S output of zlib-1.2.0/inffast.c.  Zlib-1.2.0 is in beta release at
 * the moment.  I have successfully compiled and tested this code with gcc2.96,
 * gcc3.2, icc5.0, msvc6.0.  It is very close to the speed of inffast.S
 * compiled with gcc -DNO_MMX, but inffast.S is still faster on the P3 with MMX
 * enabled.  I will attempt to merge the MMX code into this version.  Newer
 * versions of this and inffast.S can be found at
 * http://www.eetbeetee.com/zlib/ and http://www.charm.net/~christop/zlib/
 *
 */

#include <stdio.h>
#include "zutil.h"
#include "inftrees.h"
#include "inflate.h"
#include "inffast.h"

/* Mark Adler's comments from inffast.c: */

/*
   Decode literal, length, and distance codes and write out the resulting
   literal and match bytes until either not enough input or output is
   available, an end-of-block is encountered, or a data error is encountered.
   When large enough input and output buffers are supplied to inflate(), for
   example, a 16K input buffer and a 64K output buffer, more than 95% of the
   inflate execution time is spent in this routine.

   Entry assumptions:

        state->mode == LEN
        strm->avail_in >= 6
        strm->avail_out >= 258
        start >= strm->avail_out
        state->bits < 8

   On return, state->mode is one of:

        LEN -- ran out of enough output space or enough available input
        TYPE -- reached end of block code, inflate() to interpret next block
        BAD -- error in block data

   Notes:

    - The maximum input bits used by a length/distance pair is 15 bits for the
      length code, 5 bits for the length extra, 15 bits for the distance code,
      and 13 bits for the distance extra.  This totals 48 bits, or six bytes.
      Therefore if strm->avail_in >= 6, then there is enough input to avoid
      checking for available input while decoding.

    - The maximum bytes that a single length/distance pair can output is 258
      bytes, which is the maximum length that can be coded.  inflate_fast()
      requires strm->avail_out >= 258 for each loop to avoid checking for
      output space.
 */



    typedef struct inffast_ar {
/* 64   32                               x86  x86_64 */
/* ar offset                              register */
/*  0    0 */ void *esp;                /* esp save */
/*  8    4 */ void *ebp;                /* ebp save */
/* 16    8 */ unsigned char FAR *in;    /* esi rsi  local strm->next_in */
/* 24   12 */ unsigned char FAR *last;  /*     r9   while in < last */
/* 32   16 */ unsigned char FAR *out;   /* edi rdi  local strm->next_out */
/* 40   20 */ unsigned char FAR *beg;   /*          inflate()'s init next_out */
/* 48   24 */ unsigned char FAR *end;   /*     r10  while out < end */
/* 56   28 */ unsigned char FAR *window;/*          size of window, wsize!=0 */
/* 64   32 */ code const FAR *lcode;    /* ebp rbp  local strm->lencode */
/* 72   36 */ code const FAR *dcode;    /*     r11  local strm->distcode */
/* 80   40 */ size_t /*unsigned long */hold;       /* edx rdx  local strm->hold */
/* 88   44 */ unsigned bits;            /* ebx rbx  local strm->bits */
/* 92   48 */ unsigned wsize;           /*          window size */
/* 96   52 */ unsigned write;           /*          window write index */
/*100   56 */ unsigned lmask;           /*     r12  mask for lcode */
/*104   60 */ unsigned dmask;           /*     r13  mask for dcode */
/*108   64 */ unsigned len;             /*     r14  match length */
/*112   68 */ unsigned dist;            /*     r15  match distance */
/*116   72 */ unsigned status;          /*          set when state chng*/
    } type_ar;
#ifdef ASMINF

void inflate_fast(strm, start)
z_streamp strm;
unsigned start;         /* inflate()'s starting value for strm->avail_out */
{
    struct inflate_state FAR *state;
    type_ar ar;
    void inffas8664fnc(struct inffast_ar * par);



#if (defined( __GNUC__ ) && defined( __amd64__ ) && ! defined( __i386 )) || (defined(_MSC_VER) && defined(_M_AMD64))
#define PAD_AVAIL_IN 6
#define PAD_AVAIL_OUT 258
#else
#define PAD_AVAIL_IN 5
#define PAD_AVAIL_OUT 257
#endif

    /* copy state to local variables */
    state = (struct inflate_state FAR *)strm->state;

    ar.in = strm->next_in;
    ar.last = ar.in + (strm->avail_in - PAD_AVAIL_IN);
    ar.out = strm->next_out;
    ar.beg = ar.out - (start - strm->avail_out);
    ar.end = ar.out + (strm->avail_out - PAD_AVAIL_OUT);
    ar.wsize = state->wsize;
    ar.write = state->wnext;
    ar.window = state->window;
    ar.hold = state->hold;
    ar.bits = state->bits;
    ar.lcode = state->lencode;
    ar.dcode = state->distcode;
    ar.lmask = (1U << state->lenbits) - 1;
    ar.dmask = (1U << state->distbits) - 1;

    /* decode literals and length/distances until end-of-block or not enough
       input data or output space */

    /* align in on 1/2 hold size boundary */
    while (((size_t)(void *)ar.in & (sizeof(ar.hold) / 2 - 1)) != 0) {
        ar.hold += (unsigned long)*ar.in++ << ar.bits;
        ar.bits += 8;
    }

    inffas8664fnc(&ar);

    if (ar.status > 1) {
        if (ar.status == 2)
            strm->msg = "invalid literal/length code";
        else if (ar.status == 3)
            strm->msg = "invalid distance code";
        else
            strm->msg = "invalid distance too far back";
        state->mode = BAD;
    }
    else if ( ar.status == 1 ) {
        state->mode = TYPE;
    }

    /* return unused bytes (on entry, bits < 8, so in won't go too far back) */
    ar.len = ar.bits >> 3;
    ar.in -= ar.len;
    ar.bits -= ar.len << 3;
    ar.hold &= (1U << ar.bits) - 1;

    /* update state and return */
    strm->next_in = ar.in;
    strm->next_out = ar.out;
    strm->avail_in = (unsigned)(ar.in < ar.last ?
                                PAD_AVAIL_IN + (ar.last - ar.in) :
                                PAD_AVAIL_IN - (ar.in - ar.last));
    strm->avail_out = (unsigned)(ar.out < ar.end ?
                                 PAD_AVAIL_OUT + (ar.end - ar.out) :
                                 PAD_AVAIL_OUT - (ar.out - ar.end));
    state->hold = (unsigned long)ar.hold;
    state->bits = ar.bits;
    return;
}

#endif
