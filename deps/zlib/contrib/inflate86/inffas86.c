/* inffas86.c is a hand tuned assembler version of
 *
 * inffast.c -- fast decoding
 * Copyright (C) 1995-2003 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Copyright (C) 2003 Chris Anderson <christop@charm.net>
 * Please use the copyright conditions above.
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
 */

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
void inflate_fast(strm, start)
z_streamp strm;
unsigned start;         /* inflate()'s starting value for strm->avail_out */
{
    struct inflate_state FAR *state;
    struct inffast_ar {
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
/* 80   40 */ unsigned long hold;       /* edx rdx  local strm->hold */
/* 88   44 */ unsigned bits;            /* ebx rbx  local strm->bits */
/* 92   48 */ unsigned wsize;           /*          window size */
/* 96   52 */ unsigned write;           /*          window write index */
/*100   56 */ unsigned lmask;           /*     r12  mask for lcode */
/*104   60 */ unsigned dmask;           /*     r13  mask for dcode */
/*108   64 */ unsigned len;             /*     r14  match length */
/*112   68 */ unsigned dist;            /*     r15  match distance */
/*116   72 */ unsigned status;          /*          set when state chng*/
    } ar;

#if defined( __GNUC__ ) && defined( __amd64__ ) && ! defined( __i386 )
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
    while (((unsigned long)(void *)ar.in & (sizeof(ar.hold) / 2 - 1)) != 0) {
        ar.hold += (unsigned long)*ar.in++ << ar.bits;
        ar.bits += 8;
    }

#if defined( __GNUC__ ) && defined( __amd64__ ) && ! defined( __i386 )
    __asm__ __volatile__ (
"        leaq    %0, %%rax\n"
"        movq    %%rbp, 8(%%rax)\n"       /* save regs rbp and rsp */
"        movq    %%rsp, (%%rax)\n"
"        movq    %%rax, %%rsp\n"          /* make rsp point to &ar */
"        movq    16(%%rsp), %%rsi\n"      /* rsi  = in */
"        movq    32(%%rsp), %%rdi\n"      /* rdi  = out */
"        movq    24(%%rsp), %%r9\n"       /* r9   = last */
"        movq    48(%%rsp), %%r10\n"      /* r10  = end */
"        movq    64(%%rsp), %%rbp\n"      /* rbp  = lcode */
"        movq    72(%%rsp), %%r11\n"      /* r11  = dcode */
"        movq    80(%%rsp), %%rdx\n"      /* rdx  = hold */
"        movl    88(%%rsp), %%ebx\n"      /* ebx  = bits */
"        movl    100(%%rsp), %%r12d\n"    /* r12d = lmask */
"        movl    104(%%rsp), %%r13d\n"    /* r13d = dmask */
                                          /* r14d = len */
                                          /* r15d = dist */
"        cld\n"
"        cmpq    %%rdi, %%r10\n"
"        je      .L_one_time\n"           /* if only one decode left */
"        cmpq    %%rsi, %%r9\n"
"        je      .L_one_time\n"
"        jmp     .L_do_loop\n"

".L_one_time:\n"
"        movq    %%r12, %%r8\n"           /* r8 = lmask */
"        cmpb    $32, %%bl\n"
"        ja      .L_get_length_code_one_time\n"

"        lodsl\n"                         /* eax = *(uint *)in++ */
"        movb    %%bl, %%cl\n"            /* cl = bits, needs it for shifting */
"        addb    $32, %%bl\n"             /* bits += 32 */
"        shlq    %%cl, %%rax\n"
"        orq     %%rax, %%rdx\n"          /* hold |= *((uint *)in)++ << bits */
"        jmp     .L_get_length_code_one_time\n"

".align 32,0x90\n"
".L_while_test:\n"
"        cmpq    %%rdi, %%r10\n"
"        jbe     .L_break_loop\n"
"        cmpq    %%rsi, %%r9\n"
"        jbe     .L_break_loop\n"

".L_do_loop:\n"
"        movq    %%r12, %%r8\n"           /* r8 = lmask */
"        cmpb    $32, %%bl\n"
"        ja      .L_get_length_code\n"    /* if (32 < bits) */

"        lodsl\n"                         /* eax = *(uint *)in++ */
"        movb    %%bl, %%cl\n"            /* cl = bits, needs it for shifting */
"        addb    $32, %%bl\n"             /* bits += 32 */
"        shlq    %%cl, %%rax\n"
"        orq     %%rax, %%rdx\n"          /* hold |= *((uint *)in)++ << bits */

".L_get_length_code:\n"
"        andq    %%rdx, %%r8\n"            /* r8 &= hold */
"        movl    (%%rbp,%%r8,4), %%eax\n"  /* eax = lcode[hold & lmask] */

"        movb    %%ah, %%cl\n"            /* cl = this.bits */
"        subb    %%ah, %%bl\n"            /* bits -= this.bits */
"        shrq    %%cl, %%rdx\n"           /* hold >>= this.bits */

"        testb   %%al, %%al\n"
"        jnz     .L_test_for_length_base\n" /* if (op != 0) 45.7% */

"        movq    %%r12, %%r8\n"            /* r8 = lmask */
"        shrl    $16, %%eax\n"            /* output this.val char */
"        stosb\n"

".L_get_length_code_one_time:\n"
"        andq    %%rdx, %%r8\n"            /* r8 &= hold */
"        movl    (%%rbp,%%r8,4), %%eax\n" /* eax = lcode[hold & lmask] */

".L_dolen:\n"
"        movb    %%ah, %%cl\n"            /* cl = this.bits */
"        subb    %%ah, %%bl\n"            /* bits -= this.bits */
"        shrq    %%cl, %%rdx\n"           /* hold >>= this.bits */

"        testb   %%al, %%al\n"
"        jnz     .L_test_for_length_base\n" /* if (op != 0) 45.7% */

"        shrl    $16, %%eax\n"            /* output this.val char */
"        stosb\n"
"        jmp     .L_while_test\n"

".align 32,0x90\n"
".L_test_for_length_base:\n"
"        movl    %%eax, %%r14d\n"         /* len = this */
"        shrl    $16, %%r14d\n"           /* len = this.val */
"        movb    %%al, %%cl\n"

"        testb   $16, %%al\n"
"        jz      .L_test_for_second_level_length\n" /* if ((op & 16) == 0) 8% */
"        andb    $15, %%cl\n"             /* op &= 15 */
"        jz      .L_decode_distance\n"    /* if (!op) */

".L_add_bits_to_len:\n"
"        subb    %%cl, %%bl\n"
"        xorl    %%eax, %%eax\n"
"        incl    %%eax\n"
"        shll    %%cl, %%eax\n"
"        decl    %%eax\n"
"        andl    %%edx, %%eax\n"          /* eax &= hold */
"        shrq    %%cl, %%rdx\n"
"        addl    %%eax, %%r14d\n"         /* len += hold & mask[op] */

".L_decode_distance:\n"
"        movq    %%r13, %%r8\n"           /* r8 = dmask */
"        cmpb    $32, %%bl\n"
"        ja      .L_get_distance_code\n"  /* if (32 < bits) */

"        lodsl\n"                         /* eax = *(uint *)in++ */
"        movb    %%bl, %%cl\n"            /* cl = bits, needs it for shifting */
"        addb    $32, %%bl\n"             /* bits += 32 */
"        shlq    %%cl, %%rax\n"
"        orq     %%rax, %%rdx\n"          /* hold |= *((uint *)in)++ << bits */

".L_get_distance_code:\n"
"        andq    %%rdx, %%r8\n"           /* r8 &= hold */
"        movl    (%%r11,%%r8,4), %%eax\n" /* eax = dcode[hold & dmask] */

".L_dodist:\n"
"        movl    %%eax, %%r15d\n"         /* dist = this */
"        shrl    $16, %%r15d\n"           /* dist = this.val */
"        movb    %%ah, %%cl\n"
"        subb    %%ah, %%bl\n"            /* bits -= this.bits */
"        shrq    %%cl, %%rdx\n"           /* hold >>= this.bits */
"        movb    %%al, %%cl\n"            /* cl = this.op */

"        testb   $16, %%al\n"             /* if ((op & 16) == 0) */
"        jz      .L_test_for_second_level_dist\n"
"        andb    $15, %%cl\n"             /* op &= 15 */
"        jz      .L_check_dist_one\n"

".L_add_bits_to_dist:\n"
"        subb    %%cl, %%bl\n"
"        xorl    %%eax, %%eax\n"
"        incl    %%eax\n"
"        shll    %%cl, %%eax\n"
"        decl    %%eax\n"                 /* (1 << op) - 1 */
"        andl    %%edx, %%eax\n"          /* eax &= hold */
"        shrq    %%cl, %%rdx\n"
"        addl    %%eax, %%r15d\n"         /* dist += hold & ((1 << op) - 1) */

".L_check_window:\n"
"        movq    %%rsi, %%r8\n"           /* save in so from can use it's reg */
"        movq    %%rdi, %%rax\n"
"        subq    40(%%rsp), %%rax\n"      /* nbytes = out - beg */

"        cmpl    %%r15d, %%eax\n"
"        jb      .L_clip_window\n"        /* if (dist > nbytes) 4.2% */

"        movl    %%r14d, %%ecx\n"         /* ecx = len */
"        movq    %%rdi, %%rsi\n"
"        subq    %%r15, %%rsi\n"          /* from = out - dist */

"        sarl    %%ecx\n"
"        jnc     .L_copy_two\n"           /* if len % 2 == 0 */

"        rep     movsw\n"
"        movb    (%%rsi), %%al\n"
"        movb    %%al, (%%rdi)\n"
"        incq    %%rdi\n"

"        movq    %%r8, %%rsi\n"           /* move in back to %rsi, toss from */
"        jmp     .L_while_test\n"

".L_copy_two:\n"
"        rep     movsw\n"
"        movq    %%r8, %%rsi\n"           /* move in back to %rsi, toss from */
"        jmp     .L_while_test\n"

".align 32,0x90\n"
".L_check_dist_one:\n"
"        cmpl    $1, %%r15d\n"            /* if dist 1, is a memset */
"        jne     .L_check_window\n"
"        cmpq    %%rdi, 40(%%rsp)\n"      /* if out == beg, outside window */
"        je      .L_check_window\n"

"        movl    %%r14d, %%ecx\n"         /* ecx = len */
"        movb    -1(%%rdi), %%al\n"
"        movb    %%al, %%ah\n"

"        sarl    %%ecx\n"
"        jnc     .L_set_two\n"
"        movb    %%al, (%%rdi)\n"
"        incq    %%rdi\n"

".L_set_two:\n"
"        rep     stosw\n"
"        jmp     .L_while_test\n"

".align 32,0x90\n"
".L_test_for_second_level_length:\n"
"        testb   $64, %%al\n"
"        jnz     .L_test_for_end_of_block\n" /* if ((op & 64) != 0) */

"        xorl    %%eax, %%eax\n"
"        incl    %%eax\n"
"        shll    %%cl, %%eax\n"
"        decl    %%eax\n"
"        andl    %%edx, %%eax\n"         /* eax &= hold */
"        addl    %%r14d, %%eax\n"        /* eax += len */
"        movl    (%%rbp,%%rax,4), %%eax\n" /* eax = lcode[val+(hold&mask[op])]*/
"        jmp     .L_dolen\n"

".align 32,0x90\n"
".L_test_for_second_level_dist:\n"
"        testb   $64, %%al\n"
"        jnz     .L_invalid_distance_code\n" /* if ((op & 64) != 0) */

"        xorl    %%eax, %%eax\n"
"        incl    %%eax\n"
"        shll    %%cl, %%eax\n"
"        decl    %%eax\n"
"        andl    %%edx, %%eax\n"         /* eax &= hold */
"        addl    %%r15d, %%eax\n"        /* eax += dist */
"        movl    (%%r11,%%rax,4), %%eax\n" /* eax = dcode[val+(hold&mask[op])]*/
"        jmp     .L_dodist\n"

".align 32,0x90\n"
".L_clip_window:\n"
"        movl    %%eax, %%ecx\n"         /* ecx = nbytes */
"        movl    92(%%rsp), %%eax\n"     /* eax = wsize, prepare for dist cmp */
"        negl    %%ecx\n"                /* nbytes = -nbytes */

"        cmpl    %%r15d, %%eax\n"
"        jb      .L_invalid_distance_too_far\n" /* if (dist > wsize) */

"        addl    %%r15d, %%ecx\n"         /* nbytes = dist - nbytes */
"        cmpl    $0, 96(%%rsp)\n"
"        jne     .L_wrap_around_window\n" /* if (write != 0) */

"        movq    56(%%rsp), %%rsi\n"     /* from  = window */
"        subl    %%ecx, %%eax\n"         /* eax  -= nbytes */
"        addq    %%rax, %%rsi\n"         /* from += wsize - nbytes */

"        movl    %%r14d, %%eax\n"        /* eax = len */
"        cmpl    %%ecx, %%r14d\n"
"        jbe     .L_do_copy\n"           /* if (nbytes >= len) */

"        subl    %%ecx, %%eax\n"         /* eax -= nbytes */
"        rep     movsb\n"
"        movq    %%rdi, %%rsi\n"
"        subq    %%r15, %%rsi\n"         /* from = &out[ -dist ] */
"        jmp     .L_do_copy\n"

".align 32,0x90\n"
".L_wrap_around_window:\n"
"        movl    96(%%rsp), %%eax\n"     /* eax = write */
"        cmpl    %%eax, %%ecx\n"
"        jbe     .L_contiguous_in_window\n" /* if (write >= nbytes) */

"        movl    92(%%rsp), %%esi\n"     /* from  = wsize */
"        addq    56(%%rsp), %%rsi\n"     /* from += window */
"        addq    %%rax, %%rsi\n"         /* from += write */
"        subq    %%rcx, %%rsi\n"         /* from -= nbytes */
"        subl    %%eax, %%ecx\n"         /* nbytes -= write */

"        movl    %%r14d, %%eax\n"        /* eax = len */
"        cmpl    %%ecx, %%eax\n"
"        jbe     .L_do_copy\n"           /* if (nbytes >= len) */

"        subl    %%ecx, %%eax\n"         /* len -= nbytes */
"        rep     movsb\n"
"        movq    56(%%rsp), %%rsi\n"     /* from = window */
"        movl    96(%%rsp), %%ecx\n"     /* nbytes = write */
"        cmpl    %%ecx, %%eax\n"
"        jbe     .L_do_copy\n"           /* if (nbytes >= len) */

"        subl    %%ecx, %%eax\n"         /* len -= nbytes */
"        rep     movsb\n"
"        movq    %%rdi, %%rsi\n"
"        subq    %%r15, %%rsi\n"         /* from = out - dist */
"        jmp     .L_do_copy\n"

".align 32,0x90\n"
".L_contiguous_in_window:\n"
"        movq    56(%%rsp), %%rsi\n"     /* rsi = window */
"        addq    %%rax, %%rsi\n"
"        subq    %%rcx, %%rsi\n"         /* from += write - nbytes */

"        movl    %%r14d, %%eax\n"        /* eax = len */
"        cmpl    %%ecx, %%eax\n"
"        jbe     .L_do_copy\n"           /* if (nbytes >= len) */

"        subl    %%ecx, %%eax\n"         /* len -= nbytes */
"        rep     movsb\n"
"        movq    %%rdi, %%rsi\n"
"        subq    %%r15, %%rsi\n"         /* from = out - dist */
"        jmp     .L_do_copy\n"           /* if (nbytes >= len) */

".align 32,0x90\n"
".L_do_copy:\n"
"        movl    %%eax, %%ecx\n"         /* ecx = len */
"        rep     movsb\n"

"        movq    %%r8, %%rsi\n"          /* move in back to %esi, toss from */
"        jmp     .L_while_test\n"

".L_test_for_end_of_block:\n"
"        testb   $32, %%al\n"
"        jz      .L_invalid_literal_length_code\n"
"        movl    $1, 116(%%rsp)\n"
"        jmp     .L_break_loop_with_status\n"

".L_invalid_literal_length_code:\n"
"        movl    $2, 116(%%rsp)\n"
"        jmp     .L_break_loop_with_status\n"

".L_invalid_distance_code:\n"
"        movl    $3, 116(%%rsp)\n"
"        jmp     .L_break_loop_with_status\n"

".L_invalid_distance_too_far:\n"
"        movl    $4, 116(%%rsp)\n"
"        jmp     .L_break_loop_with_status\n"

".L_break_loop:\n"
"        movl    $0, 116(%%rsp)\n"

".L_break_loop_with_status:\n"
/* put in, out, bits, and hold back into ar and pop esp */
"        movq    %%rsi, 16(%%rsp)\n"     /* in */
"        movq    %%rdi, 32(%%rsp)\n"     /* out */
"        movl    %%ebx, 88(%%rsp)\n"     /* bits */
"        movq    %%rdx, 80(%%rsp)\n"     /* hold */
"        movq    (%%rsp), %%rax\n"       /* restore rbp and rsp */
"        movq    8(%%rsp), %%rbp\n"
"        movq    %%rax, %%rsp\n"
          :
          : "m" (ar)
          : "memory", "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi",
            "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"
    );
#elif ( defined( __GNUC__ ) || defined( __ICC ) ) && defined( __i386 )
    __asm__ __volatile__ (
"        leal    %0, %%eax\n"
"        movl    %%esp, (%%eax)\n"        /* save esp, ebp */
"        movl    %%ebp, 4(%%eax)\n"
"        movl    %%eax, %%esp\n"
"        movl    8(%%esp), %%esi\n"       /* esi = in */
"        movl    16(%%esp), %%edi\n"      /* edi = out */
"        movl    40(%%esp), %%edx\n"      /* edx = hold */
"        movl    44(%%esp), %%ebx\n"      /* ebx = bits */
"        movl    32(%%esp), %%ebp\n"      /* ebp = lcode */

"        cld\n"
"        jmp     .L_do_loop\n"

".align 32,0x90\n"
".L_while_test:\n"
"        cmpl    %%edi, 24(%%esp)\n"      /* out < end */
"        jbe     .L_break_loop\n"
"        cmpl    %%esi, 12(%%esp)\n"      /* in < last */
"        jbe     .L_break_loop\n"

".L_do_loop:\n"
"        cmpb    $15, %%bl\n"
"        ja      .L_get_length_code\n"    /* if (15 < bits) */

"        xorl    %%eax, %%eax\n"
"        lodsw\n"                         /* al = *(ushort *)in++ */
"        movb    %%bl, %%cl\n"            /* cl = bits, needs it for shifting */
"        addb    $16, %%bl\n"             /* bits += 16 */
"        shll    %%cl, %%eax\n"
"        orl     %%eax, %%edx\n"        /* hold |= *((ushort *)in)++ << bits */

".L_get_length_code:\n"
"        movl    56(%%esp), %%eax\n"      /* eax = lmask */
"        andl    %%edx, %%eax\n"          /* eax &= hold */
"        movl    (%%ebp,%%eax,4), %%eax\n" /* eax = lcode[hold & lmask] */

".L_dolen:\n"
"        movb    %%ah, %%cl\n"            /* cl = this.bits */
"        subb    %%ah, %%bl\n"            /* bits -= this.bits */
"        shrl    %%cl, %%edx\n"           /* hold >>= this.bits */

"        testb   %%al, %%al\n"
"        jnz     .L_test_for_length_base\n" /* if (op != 0) 45.7% */

"        shrl    $16, %%eax\n"            /* output this.val char */
"        stosb\n"
"        jmp     .L_while_test\n"

".align 32,0x90\n"
".L_test_for_length_base:\n"
"        movl    %%eax, %%ecx\n"          /* len = this */
"        shrl    $16, %%ecx\n"            /* len = this.val */
"        movl    %%ecx, 64(%%esp)\n"      /* save len */
"        movb    %%al, %%cl\n"

"        testb   $16, %%al\n"
"        jz      .L_test_for_second_level_length\n" /* if ((op & 16) == 0) 8% */
"        andb    $15, %%cl\n"             /* op &= 15 */
"        jz      .L_decode_distance\n"    /* if (!op) */
"        cmpb    %%cl, %%bl\n"
"        jae     .L_add_bits_to_len\n"    /* if (op <= bits) */

"        movb    %%cl, %%ch\n"            /* stash op in ch, freeing cl */
"        xorl    %%eax, %%eax\n"
"        lodsw\n"                         /* al = *(ushort *)in++ */
"        movb    %%bl, %%cl\n"            /* cl = bits, needs it for shifting */
"        addb    $16, %%bl\n"             /* bits += 16 */
"        shll    %%cl, %%eax\n"
"        orl     %%eax, %%edx\n"         /* hold |= *((ushort *)in)++ << bits */
"        movb    %%ch, %%cl\n"            /* move op back to ecx */

".L_add_bits_to_len:\n"
"        subb    %%cl, %%bl\n"
"        xorl    %%eax, %%eax\n"
"        incl    %%eax\n"
"        shll    %%cl, %%eax\n"
"        decl    %%eax\n"
"        andl    %%edx, %%eax\n"          /* eax &= hold */
"        shrl    %%cl, %%edx\n"
"        addl    %%eax, 64(%%esp)\n"      /* len += hold & mask[op] */

".L_decode_distance:\n"
"        cmpb    $15, %%bl\n"
"        ja      .L_get_distance_code\n"  /* if (15 < bits) */

"        xorl    %%eax, %%eax\n"
"        lodsw\n"                         /* al = *(ushort *)in++ */
"        movb    %%bl, %%cl\n"            /* cl = bits, needs it for shifting */
"        addb    $16, %%bl\n"             /* bits += 16 */
"        shll    %%cl, %%eax\n"
"        orl     %%eax, %%edx\n"         /* hold |= *((ushort *)in)++ << bits */

".L_get_distance_code:\n"
"        movl    60(%%esp), %%eax\n"      /* eax = dmask */
"        movl    36(%%esp), %%ecx\n"      /* ecx = dcode */
"        andl    %%edx, %%eax\n"          /* eax &= hold */
"        movl    (%%ecx,%%eax,4), %%eax\n"/* eax = dcode[hold & dmask] */

".L_dodist:\n"
"        movl    %%eax, %%ebp\n"          /* dist = this */
"        shrl    $16, %%ebp\n"            /* dist = this.val */
"        movb    %%ah, %%cl\n"
"        subb    %%ah, %%bl\n"            /* bits -= this.bits */
"        shrl    %%cl, %%edx\n"           /* hold >>= this.bits */
"        movb    %%al, %%cl\n"            /* cl = this.op */

"        testb   $16, %%al\n"             /* if ((op & 16) == 0) */
"        jz      .L_test_for_second_level_dist\n"
"        andb    $15, %%cl\n"             /* op &= 15 */
"        jz      .L_check_dist_one\n"
"        cmpb    %%cl, %%bl\n"
"        jae     .L_add_bits_to_dist\n"   /* if (op <= bits) 97.6% */

"        movb    %%cl, %%ch\n"            /* stash op in ch, freeing cl */
"        xorl    %%eax, %%eax\n"
"        lodsw\n"                         /* al = *(ushort *)in++ */
"        movb    %%bl, %%cl\n"            /* cl = bits, needs it for shifting */
"        addb    $16, %%bl\n"             /* bits += 16 */
"        shll    %%cl, %%eax\n"
"        orl     %%eax, %%edx\n"        /* hold |= *((ushort *)in)++ << bits */
"        movb    %%ch, %%cl\n"            /* move op back to ecx */

".L_add_bits_to_dist:\n"
"        subb    %%cl, %%bl\n"
"        xorl    %%eax, %%eax\n"
"        incl    %%eax\n"
"        shll    %%cl, %%eax\n"
"        decl    %%eax\n"                 /* (1 << op) - 1 */
"        andl    %%edx, %%eax\n"          /* eax &= hold */
"        shrl    %%cl, %%edx\n"
"        addl    %%eax, %%ebp\n"          /* dist += hold & ((1 << op) - 1) */

".L_check_window:\n"
"        movl    %%esi, 8(%%esp)\n"       /* save in so from can use it's reg */
"        movl    %%edi, %%eax\n"
"        subl    20(%%esp), %%eax\n"      /* nbytes = out - beg */

"        cmpl    %%ebp, %%eax\n"
"        jb      .L_clip_window\n"        /* if (dist > nbytes) 4.2% */

"        movl    64(%%esp), %%ecx\n"      /* ecx = len */
"        movl    %%edi, %%esi\n"
"        subl    %%ebp, %%esi\n"          /* from = out - dist */

"        sarl    %%ecx\n"
"        jnc     .L_copy_two\n"           /* if len % 2 == 0 */

"        rep     movsw\n"
"        movb    (%%esi), %%al\n"
"        movb    %%al, (%%edi)\n"
"        incl    %%edi\n"

"        movl    8(%%esp), %%esi\n"       /* move in back to %esi, toss from */
"        movl    32(%%esp), %%ebp\n"      /* ebp = lcode */
"        jmp     .L_while_test\n"

".L_copy_two:\n"
"        rep     movsw\n"
"        movl    8(%%esp), %%esi\n"       /* move in back to %esi, toss from */
"        movl    32(%%esp), %%ebp\n"      /* ebp = lcode */
"        jmp     .L_while_test\n"

".align 32,0x90\n"
".L_check_dist_one:\n"
"        cmpl    $1, %%ebp\n"            /* if dist 1, is a memset */
"        jne     .L_check_window\n"
"        cmpl    %%edi, 20(%%esp)\n"
"        je      .L_check_window\n"      /* out == beg, if outside window */

"        movl    64(%%esp), %%ecx\n"      /* ecx = len */
"        movb    -1(%%edi), %%al\n"
"        movb    %%al, %%ah\n"

"        sarl    %%ecx\n"
"        jnc     .L_set_two\n"
"        movb    %%al, (%%edi)\n"
"        incl    %%edi\n"

".L_set_two:\n"
"        rep     stosw\n"
"        movl    32(%%esp), %%ebp\n"      /* ebp = lcode */
"        jmp     .L_while_test\n"

".align 32,0x90\n"
".L_test_for_second_level_length:\n"
"        testb   $64, %%al\n"
"        jnz     .L_test_for_end_of_block\n" /* if ((op & 64) != 0) */

"        xorl    %%eax, %%eax\n"
"        incl    %%eax\n"
"        shll    %%cl, %%eax\n"
"        decl    %%eax\n"
"        andl    %%edx, %%eax\n"         /* eax &= hold */
"        addl    64(%%esp), %%eax\n"     /* eax += len */
"        movl    (%%ebp,%%eax,4), %%eax\n" /* eax = lcode[val+(hold&mask[op])]*/
"        jmp     .L_dolen\n"

".align 32,0x90\n"
".L_test_for_second_level_dist:\n"
"        testb   $64, %%al\n"
"        jnz     .L_invalid_distance_code\n" /* if ((op & 64) != 0) */

"        xorl    %%eax, %%eax\n"
"        incl    %%eax\n"
"        shll    %%cl, %%eax\n"
"        decl    %%eax\n"
"        andl    %%edx, %%eax\n"         /* eax &= hold */
"        addl    %%ebp, %%eax\n"         /* eax += dist */
"        movl    36(%%esp), %%ecx\n"     /* ecx = dcode */
"        movl    (%%ecx,%%eax,4), %%eax\n" /* eax = dcode[val+(hold&mask[op])]*/
"        jmp     .L_dodist\n"

".align 32,0x90\n"
".L_clip_window:\n"
"        movl    %%eax, %%ecx\n"
"        movl    48(%%esp), %%eax\n"     /* eax = wsize */
"        negl    %%ecx\n"                /* nbytes = -nbytes */
"        movl    28(%%esp), %%esi\n"     /* from = window */

"        cmpl    %%ebp, %%eax\n"
"        jb      .L_invalid_distance_too_far\n" /* if (dist > wsize) */

"        addl    %%ebp, %%ecx\n"         /* nbytes = dist - nbytes */
"        cmpl    $0, 52(%%esp)\n"
"        jne     .L_wrap_around_window\n" /* if (write != 0) */

"        subl    %%ecx, %%eax\n"
"        addl    %%eax, %%esi\n"         /* from += wsize - nbytes */

"        movl    64(%%esp), %%eax\n"     /* eax = len */
"        cmpl    %%ecx, %%eax\n"
"        jbe     .L_do_copy\n"           /* if (nbytes >= len) */

"        subl    %%ecx, %%eax\n"         /* len -= nbytes */
"        rep     movsb\n"
"        movl    %%edi, %%esi\n"
"        subl    %%ebp, %%esi\n"         /* from = out - dist */
"        jmp     .L_do_copy\n"

".align 32,0x90\n"
".L_wrap_around_window:\n"
"        movl    52(%%esp), %%eax\n"     /* eax = write */
"        cmpl    %%eax, %%ecx\n"
"        jbe     .L_contiguous_in_window\n" /* if (write >= nbytes) */

"        addl    48(%%esp), %%esi\n"     /* from += wsize */
"        addl    %%eax, %%esi\n"         /* from += write */
"        subl    %%ecx, %%esi\n"         /* from -= nbytes */
"        subl    %%eax, %%ecx\n"         /* nbytes -= write */

"        movl    64(%%esp), %%eax\n"     /* eax = len */
"        cmpl    %%ecx, %%eax\n"
"        jbe     .L_do_copy\n"           /* if (nbytes >= len) */

"        subl    %%ecx, %%eax\n"         /* len -= nbytes */
"        rep     movsb\n"
"        movl    28(%%esp), %%esi\n"     /* from = window */
"        movl    52(%%esp), %%ecx\n"     /* nbytes = write */
"        cmpl    %%ecx, %%eax\n"
"        jbe     .L_do_copy\n"           /* if (nbytes >= len) */

"        subl    %%ecx, %%eax\n"         /* len -= nbytes */
"        rep     movsb\n"
"        movl    %%edi, %%esi\n"
"        subl    %%ebp, %%esi\n"         /* from = out - dist */
"        jmp     .L_do_copy\n"

".align 32,0x90\n"
".L_contiguous_in_window:\n"
"        addl    %%eax, %%esi\n"
"        subl    %%ecx, %%esi\n"         /* from += write - nbytes */

"        movl    64(%%esp), %%eax\n"     /* eax = len */
"        cmpl    %%ecx, %%eax\n"
"        jbe     .L_do_copy\n"           /* if (nbytes >= len) */

"        subl    %%ecx, %%eax\n"         /* len -= nbytes */
"        rep     movsb\n"
"        movl    %%edi, %%esi\n"
"        subl    %%ebp, %%esi\n"         /* from = out - dist */
"        jmp     .L_do_copy\n"           /* if (nbytes >= len) */

".align 32,0x90\n"
".L_do_copy:\n"
"        movl    %%eax, %%ecx\n"
"        rep     movsb\n"

"        movl    8(%%esp), %%esi\n"      /* move in back to %esi, toss from */
"        movl    32(%%esp), %%ebp\n"     /* ebp = lcode */
"        jmp     .L_while_test\n"

".L_test_for_end_of_block:\n"
"        testb   $32, %%al\n"
"        jz      .L_invalid_literal_length_code\n"
"        movl    $1, 72(%%esp)\n"
"        jmp     .L_break_loop_with_status\n"

".L_invalid_literal_length_code:\n"
"        movl    $2, 72(%%esp)\n"
"        jmp     .L_break_loop_with_status\n"

".L_invalid_distance_code:\n"
"        movl    $3, 72(%%esp)\n"
"        jmp     .L_break_loop_with_status\n"

".L_invalid_distance_too_far:\n"
"        movl    8(%%esp), %%esi\n"
"        movl    $4, 72(%%esp)\n"
"        jmp     .L_break_loop_with_status\n"

".L_break_loop:\n"
"        movl    $0, 72(%%esp)\n"

".L_break_loop_with_status:\n"
/* put in, out, bits, and hold back into ar and pop esp */
"        movl    %%esi, 8(%%esp)\n"      /* save in */
"        movl    %%edi, 16(%%esp)\n"     /* save out */
"        movl    %%ebx, 44(%%esp)\n"     /* save bits */
"        movl    %%edx, 40(%%esp)\n"     /* save hold */
"        movl    4(%%esp), %%ebp\n"      /* restore esp, ebp */
"        movl    (%%esp), %%esp\n"
          :
          : "m" (ar)
          : "memory", "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"
    );
#elif defined( _MSC_VER ) && ! defined( _M_AMD64 )
    __asm {
	lea	eax, ar
	mov	[eax], esp         /* save esp, ebp */
	mov	[eax+4], ebp
	mov	esp, eax
	mov	esi, [esp+8]       /* esi = in */
	mov	edi, [esp+16]      /* edi = out */
	mov	edx, [esp+40]      /* edx = hold */
	mov	ebx, [esp+44]      /* ebx = bits */
	mov	ebp, [esp+32]      /* ebp = lcode */

	cld
	jmp	L_do_loop

ALIGN 4
L_while_test:
	cmp	[esp+24], edi
	jbe	L_break_loop
	cmp	[esp+12], esi
	jbe	L_break_loop

L_do_loop:
	cmp	bl, 15
	ja	L_get_length_code    /* if (15 < bits) */

	xor	eax, eax
	lodsw                         /* al = *(ushort *)in++ */
	mov	cl, bl            /* cl = bits, needs it for shifting */
	add	bl, 16             /* bits += 16 */
	shl	eax, cl
	or	edx, eax        /* hold |= *((ushort *)in)++ << bits */

L_get_length_code:
	mov	eax, [esp+56]      /* eax = lmask */
	and	eax, edx          /* eax &= hold */
	mov	eax, [ebp+eax*4] /* eax = lcode[hold & lmask] */

L_dolen:
	mov	cl, ah            /* cl = this.bits */
	sub	bl, ah            /* bits -= this.bits */
	shr	edx, cl           /* hold >>= this.bits */

	test	al, al
	jnz	L_test_for_length_base /* if (op != 0) 45.7% */

	shr	eax, 16            /* output this.val char */
	stosb
	jmp	L_while_test

ALIGN 4
L_test_for_length_base:
	mov	ecx, eax          /* len = this */
	shr	ecx, 16            /* len = this.val */
	mov	[esp+64], ecx      /* save len */
	mov	cl, al

	test	al, 16
	jz	L_test_for_second_level_length /* if ((op & 16) == 0) 8% */
	and	cl, 15             /* op &= 15 */
	jz	L_decode_distance    /* if (!op) */
	cmp	bl, cl
	jae	L_add_bits_to_len    /* if (op <= bits) */

	mov	ch, cl            /* stash op in ch, freeing cl */
	xor	eax, eax
	lodsw                         /* al = *(ushort *)in++ */
	mov	cl, bl            /* cl = bits, needs it for shifting */
	add	bl, 16             /* bits += 16 */
	shl	eax, cl
	or	edx, eax         /* hold |= *((ushort *)in)++ << bits */
	mov	cl, ch            /* move op back to ecx */

L_add_bits_to_len:
	sub	bl, cl
	xor	eax, eax
	inc	eax
	shl	eax, cl
	dec	eax
	and	eax, edx          /* eax &= hold */
	shr	edx, cl
	add	[esp+64], eax      /* len += hold & mask[op] */

L_decode_distance:
	cmp	bl, 15
	ja	L_get_distance_code  /* if (15 < bits) */

	xor	eax, eax
	lodsw                         /* al = *(ushort *)in++ */
	mov	cl, bl            /* cl = bits, needs it for shifting */
	add	bl, 16             /* bits += 16 */
	shl	eax, cl
	or	edx, eax         /* hold |= *((ushort *)in)++ << bits */

L_get_distance_code:
	mov	eax, [esp+60]      /* eax = dmask */
	mov	ecx, [esp+36]      /* ecx = dcode */
	and	eax, edx          /* eax &= hold */
	mov	eax, [ecx+eax*4]/* eax = dcode[hold & dmask] */

L_dodist:
	mov	ebp, eax          /* dist = this */
	shr	ebp, 16            /* dist = this.val */
	mov	cl, ah
	sub	bl, ah            /* bits -= this.bits */
	shr	edx, cl           /* hold >>= this.bits */
	mov	cl, al            /* cl = this.op */

	test	al, 16             /* if ((op & 16) == 0) */
	jz	L_test_for_second_level_dist
	and	cl, 15             /* op &= 15 */
	jz	L_check_dist_one
	cmp	bl, cl
	jae	L_add_bits_to_dist   /* if (op <= bits) 97.6% */

	mov	ch, cl            /* stash op in ch, freeing cl */
	xor	eax, eax
	lodsw                         /* al = *(ushort *)in++ */
	mov	cl, bl            /* cl = bits, needs it for shifting */
	add	bl, 16             /* bits += 16 */
	shl	eax, cl
	or	edx, eax        /* hold |= *((ushort *)in)++ << bits */
	mov	cl, ch            /* move op back to ecx */

L_add_bits_to_dist:
	sub	bl, cl
	xor	eax, eax
	inc	eax
	shl	eax, cl
	dec	eax                 /* (1 << op) - 1 */
	and	eax, edx          /* eax &= hold */
	shr	edx, cl
	add	ebp, eax          /* dist += hold & ((1 << op) - 1) */

L_check_window:
	mov	[esp+8], esi       /* save in so from can use it's reg */
	mov	eax, edi
	sub	eax, [esp+20]      /* nbytes = out - beg */

	cmp	eax, ebp
	jb	L_clip_window        /* if (dist > nbytes) 4.2% */

	mov	ecx, [esp+64]      /* ecx = len */
	mov	esi, edi
	sub	esi, ebp          /* from = out - dist */

	sar	ecx, 1
	jnc	L_copy_two

	rep     movsw
	mov	al, [esi]
	mov	[edi], al
	inc	edi

	mov	esi, [esp+8]      /* move in back to %esi, toss from */
	mov	ebp, [esp+32]     /* ebp = lcode */
	jmp	L_while_test

L_copy_two:
	rep     movsw
	mov	esi, [esp+8]      /* move in back to %esi, toss from */
	mov	ebp, [esp+32]     /* ebp = lcode */
	jmp	L_while_test

ALIGN 4
L_check_dist_one:
	cmp	ebp, 1            /* if dist 1, is a memset */
	jne	L_check_window
	cmp	[esp+20], edi
	je	L_check_window    /* out == beg, if outside window */

	mov	ecx, [esp+64]     /* ecx = len */
	mov	al, [edi-1]
	mov	ah, al

	sar	ecx, 1
	jnc	L_set_two
	mov	[edi], al         /* memset out with from[-1] */
	inc	edi

L_set_two:
	rep     stosw
	mov	ebp, [esp+32]     /* ebp = lcode */
	jmp	L_while_test

ALIGN 4
L_test_for_second_level_length:
	test	al, 64
	jnz	L_test_for_end_of_block /* if ((op & 64) != 0) */

	xor	eax, eax
	inc	eax
	shl	eax, cl
	dec	eax
	and	eax, edx         /* eax &= hold */
	add	eax, [esp+64]     /* eax += len */
	mov	eax, [ebp+eax*4] /* eax = lcode[val+(hold&mask[op])]*/
	jmp	L_dolen

ALIGN 4
L_test_for_second_level_dist:
	test	al, 64
	jnz	L_invalid_distance_code /* if ((op & 64) != 0) */

	xor	eax, eax
	inc	eax
	shl	eax, cl
	dec	eax
	and	eax, edx         /* eax &= hold */
	add	eax, ebp         /* eax += dist */
	mov	ecx, [esp+36]     /* ecx = dcode */
	mov	eax, [ecx+eax*4] /* eax = dcode[val+(hold&mask[op])]*/
	jmp	L_dodist

ALIGN 4
L_clip_window:
	mov	ecx, eax
	mov	eax, [esp+48]     /* eax = wsize */
	neg	ecx                /* nbytes = -nbytes */
	mov	esi, [esp+28]     /* from = window */

	cmp	eax, ebp
	jb	L_invalid_distance_too_far /* if (dist > wsize) */

	add	ecx, ebp         /* nbytes = dist - nbytes */
	cmp	dword ptr [esp+52], 0
	jne	L_wrap_around_window /* if (write != 0) */

	sub	eax, ecx
	add	esi, eax         /* from += wsize - nbytes */

	mov	eax, [esp+64]    /* eax = len */
	cmp	eax, ecx
	jbe	L_do_copy          /* if (nbytes >= len) */

	sub	eax, ecx         /* len -= nbytes */
	rep     movsb
	mov	esi, edi
	sub	esi, ebp         /* from = out - dist */
	jmp	L_do_copy

ALIGN 4
L_wrap_around_window:
	mov	eax, [esp+52]    /* eax = write */
	cmp	ecx, eax
	jbe	L_contiguous_in_window /* if (write >= nbytes) */

	add	esi, [esp+48]    /* from += wsize */
	add	esi, eax         /* from += write */
	sub	esi, ecx         /* from -= nbytes */
	sub	ecx, eax         /* nbytes -= write */

	mov	eax, [esp+64]    /* eax = len */
	cmp	eax, ecx
	jbe	L_do_copy          /* if (nbytes >= len) */

	sub	eax, ecx         /* len -= nbytes */
	rep     movsb
	mov	esi, [esp+28]     /* from = window */
	mov	ecx, [esp+52]     /* nbytes = write */
	cmp	eax, ecx
	jbe	L_do_copy          /* if (nbytes >= len) */

	sub	eax, ecx         /* len -= nbytes */
	rep     movsb
	mov	esi, edi
	sub	esi, ebp         /* from = out - dist */
	jmp	L_do_copy

ALIGN 4
L_contiguous_in_window:
	add	esi, eax
	sub	esi, ecx         /* from += write - nbytes */

	mov	eax, [esp+64]    /* eax = len */
	cmp	eax, ecx
	jbe	L_do_copy          /* if (nbytes >= len) */

	sub	eax, ecx         /* len -= nbytes */
	rep     movsb
	mov	esi, edi
	sub	esi, ebp         /* from = out - dist */
	jmp	L_do_copy

ALIGN 4
L_do_copy:
	mov	ecx, eax
	rep     movsb

	mov	esi, [esp+8]      /* move in back to %esi, toss from */
	mov	ebp, [esp+32]     /* ebp = lcode */
	jmp	L_while_test

L_test_for_end_of_block:
	test	al, 32
	jz	L_invalid_literal_length_code
	mov	dword ptr [esp+72], 1
	jmp	L_break_loop_with_status

L_invalid_literal_length_code:
	mov	dword ptr [esp+72], 2
	jmp	L_break_loop_with_status

L_invalid_distance_code:
	mov	dword ptr [esp+72], 3
	jmp	L_break_loop_with_status

L_invalid_distance_too_far:
	mov	esi, [esp+4]
	mov	dword ptr [esp+72], 4
	jmp	L_break_loop_with_status

L_break_loop:
	mov	dword ptr [esp+72], 0

L_break_loop_with_status:
/* put in, out, bits, and hold back into ar and pop esp */
	mov	[esp+8], esi     /* save in */
	mov	[esp+16], edi    /* save out */
	mov	[esp+44], ebx    /* save bits */
	mov	[esp+40], edx    /* save hold */
	mov	ebp, [esp+4]     /* restore esp, ebp */
	mov	esp, [esp]
    }
#else
#error "x86 architecture not defined"
#endif

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
    state->hold = ar.hold;
    state->bits = ar.bits;
    return;
}

