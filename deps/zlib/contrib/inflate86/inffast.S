/*
 * inffast.S is a hand tuned assembler version of:
 *
 * inffast.c -- fast decoding
 * Copyright (C) 1995-2003 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Copyright (C) 2003 Chris Anderson <christop@charm.net>
 * Please use the copyright conditions above.
 *
 * This version (Jan-23-2003) of inflate_fast was coded and tested under
 * GNU/Linux on a pentium 3, using the gcc-3.2 compiler distribution.  On that
 * machine, I found that gzip style archives decompressed about 20% faster than
 * the gcc-3.2 -O3 -fomit-frame-pointer compiled version.  Your results will
 * depend on how large of a buffer is used for z_stream.next_in & next_out
 * (8K-32K worked best for my 256K cpu cache) and how much overhead there is in
 * stream processing I/O and crc32/addler32.  In my case, this routine used
 * 70% of the cpu time and crc32 used 20%.
 *
 * I am confident that this version will work in the general case, but I have
 * not tested a wide variety of datasets or a wide variety of platforms.
 *
 * Jan-24-2003 -- Added -DUSE_MMX define for slightly faster inflating.
 * It should be a runtime flag instead of compile time flag...
 *
 * Jan-26-2003 -- Added runtime check for MMX support with cpuid instruction.
 * With -DUSE_MMX, only MMX code is compiled.  With -DNO_MMX, only non-MMX code
 * is compiled.  Without either option, runtime detection is enabled.  Runtime
 * detection should work on all modern cpus and the recomended algorithm (flip
 * ID bit on eflags and then use the cpuid instruction) is used in many
 * multimedia applications.  Tested under win2k with gcc-2.95 and gas-2.12
 * distributed with cygwin3.  Compiling with gcc-2.95 -c inffast.S -o
 * inffast.obj generates a COFF object which can then be linked with MSVC++
 * compiled code.  Tested under FreeBSD 4.7 with gcc-2.95.
 *
 * Jan-28-2003 -- Tested Athlon XP... MMX mode is slower than no MMX (and
 * slower than compiler generated code).  Adjusted cpuid check to use the MMX
 * code only for Pentiums < P4 until I have more data on the P4.  Speed
 * improvment is only about 15% on the Athlon when compared with code generated
 * with MSVC++.  Not sure yet, but I think the P4 will also be slower using the
 * MMX mode because many of it's x86 ALU instructions execute in .5 cycles and
 * have less latency than MMX ops.  Added code to buffer the last 11 bytes of
 * the input stream since the MMX code grabs bits in chunks of 32, which
 * differs from the inffast.c algorithm.  I don't think there would have been
 * read overruns where a page boundary was crossed (a segfault), but there
 * could have been overruns when next_in ends on unaligned memory (unintialized
 * memory read).
 *
 * Mar-13-2003 -- P4 MMX is slightly slower than P4 NO_MMX.  I created a C
 * version of the non-MMX code so that it doesn't depend on zstrm and zstate
 * structure offsets which are hard coded in this file.  This was last tested
 * with zlib-1.2.0 which is currently in beta testing, newer versions of this
 * and inffas86.c can be found at http://www.eetbeetee.com/zlib/ and
 * http://www.charm.net/~christop/zlib/
 */


/*
 * if you have underscore linking problems (_inflate_fast undefined), try
 * using -DGAS_COFF
 */
#if ! defined( GAS_COFF ) && ! defined( GAS_ELF )

#if defined( WIN32 ) || defined( __CYGWIN__ )
#define GAS_COFF /* windows object format */
#else
#define GAS_ELF
#endif

#endif /* ! GAS_COFF && ! GAS_ELF */


#if defined( GAS_COFF )

/* coff externals have underscores */
#define inflate_fast _inflate_fast
#define inflate_fast_use_mmx _inflate_fast_use_mmx

#endif /* GAS_COFF */


.file "inffast.S"

.globl inflate_fast

.text
.align 4,0
.L_invalid_literal_length_code_msg:
.string "invalid literal/length code"

.align 4,0
.L_invalid_distance_code_msg:
.string "invalid distance code"

.align 4,0
.L_invalid_distance_too_far_msg:
.string "invalid distance too far back"

#if ! defined( NO_MMX )
.align 4,0
.L_mask: /* mask[N] = ( 1 << N ) - 1 */
.long 0
.long 1
.long 3
.long 7
.long 15
.long 31
.long 63
.long 127
.long 255
.long 511
.long 1023
.long 2047
.long 4095
.long 8191
.long 16383
.long 32767
.long 65535
.long 131071
.long 262143
.long 524287
.long 1048575
.long 2097151
.long 4194303
.long 8388607
.long 16777215
.long 33554431
.long 67108863
.long 134217727
.long 268435455
.long 536870911
.long 1073741823
.long 2147483647
.long 4294967295
#endif /* NO_MMX */

.text

/*
 * struct z_stream offsets, in zlib.h
 */
#define next_in_strm   0   /* strm->next_in */
#define avail_in_strm  4   /* strm->avail_in */
#define next_out_strm  12  /* strm->next_out */
#define avail_out_strm 16  /* strm->avail_out */
#define msg_strm       24  /* strm->msg */
#define state_strm     28  /* strm->state */

/*
 * struct inflate_state offsets, in inflate.h
 */
#define mode_state     0   /* state->mode */
#define wsize_state    32  /* state->wsize */
#define write_state    40  /* state->write */
#define window_state   44  /* state->window */
#define hold_state     48  /* state->hold */
#define bits_state     52  /* state->bits */
#define lencode_state  68  /* state->lencode */
#define distcode_state 72  /* state->distcode */
#define lenbits_state  76  /* state->lenbits */
#define distbits_state 80  /* state->distbits */

/*
 * inflate_fast's activation record
 */
#define local_var_size 64 /* how much local space for vars */
#define strm_sp        88 /* first arg: z_stream * (local_var_size + 24) */
#define start_sp       92 /* second arg: unsigned int (local_var_size + 28) */

/*
 * offsets for local vars on stack
 */
#define out            60  /* unsigned char* */
#define window         56  /* unsigned char* */
#define wsize          52  /* unsigned int */
#define write          48  /* unsigned int */
#define in             44  /* unsigned char* */
#define beg            40  /* unsigned char* */
#define buf            28  /* char[ 12 ] */
#define len            24  /* unsigned int */
#define last           20  /* unsigned char* */
#define end            16  /* unsigned char* */
#define dcode          12  /* code* */
#define lcode           8  /* code* */
#define dmask           4  /* unsigned int */
#define lmask           0  /* unsigned int */

/*
 * typedef enum inflate_mode consts, in inflate.h
 */
#define INFLATE_MODE_TYPE 11  /* state->mode flags enum-ed in inflate.h */
#define INFLATE_MODE_BAD  26


#if ! defined( USE_MMX ) && ! defined( NO_MMX )

#define RUN_TIME_MMX

#define CHECK_MMX    1
#define DO_USE_MMX   2
#define DONT_USE_MMX 3

.globl inflate_fast_use_mmx

.data

.align 4,0
inflate_fast_use_mmx: /* integer flag for run time control 1=check,2=mmx,3=no */
.long CHECK_MMX

#if defined( GAS_ELF )
/* elf info */
.type   inflate_fast_use_mmx,@object
.size   inflate_fast_use_mmx,4
#endif

#endif /* RUN_TIME_MMX */

#if defined( GAS_COFF )
/* coff info: scl 2 = extern, type 32 = function */
.def inflate_fast; .scl 2; .type 32; .endef
#endif

.text

.align 32,0x90
inflate_fast:
        pushl   %edi
        pushl   %esi
        pushl   %ebp
        pushl   %ebx
        pushf   /* save eflags (strm_sp, state_sp assumes this is 32 bits) */
        subl    $local_var_size, %esp
        cld

#define strm_r  %esi
#define state_r %edi

        movl    strm_sp(%esp), strm_r
        movl    state_strm(strm_r), state_r

        /* in = strm->next_in;
         * out = strm->next_out;
         * last = in + strm->avail_in - 11;
         * beg = out - (start - strm->avail_out);
         * end = out + (strm->avail_out - 257);
         */
        movl    avail_in_strm(strm_r), %edx
        movl    next_in_strm(strm_r), %eax

        addl    %eax, %edx      /* avail_in += next_in */
        subl    $11, %edx       /* avail_in -= 11 */

        movl    %eax, in(%esp)
        movl    %edx, last(%esp)

        movl    start_sp(%esp), %ebp
        movl    avail_out_strm(strm_r), %ecx
        movl    next_out_strm(strm_r), %ebx

        subl    %ecx, %ebp      /* start -= avail_out */
        negl    %ebp            /* start = -start */
        addl    %ebx, %ebp      /* start += next_out */

        subl    $257, %ecx      /* avail_out -= 257 */
        addl    %ebx, %ecx      /* avail_out += out */

        movl    %ebx, out(%esp)
        movl    %ebp, beg(%esp)
        movl    %ecx, end(%esp)

        /* wsize = state->wsize;
         * write = state->write;
         * window = state->window;
         * hold = state->hold;
         * bits = state->bits;
         * lcode = state->lencode;
         * dcode = state->distcode;
         * lmask = ( 1 << state->lenbits ) - 1;
         * dmask = ( 1 << state->distbits ) - 1;
         */

        movl    lencode_state(state_r), %eax
        movl    distcode_state(state_r), %ecx

        movl    %eax, lcode(%esp)
        movl    %ecx, dcode(%esp)

        movl    $1, %eax
        movl    lenbits_state(state_r), %ecx
        shll    %cl, %eax
        decl    %eax
        movl    %eax, lmask(%esp)

        movl    $1, %eax
        movl    distbits_state(state_r), %ecx
        shll    %cl, %eax
        decl    %eax
        movl    %eax, dmask(%esp)

        movl    wsize_state(state_r), %eax
        movl    write_state(state_r), %ecx
        movl    window_state(state_r), %edx

        movl    %eax, wsize(%esp)
        movl    %ecx, write(%esp)
        movl    %edx, window(%esp)

        movl    hold_state(state_r), %ebp
        movl    bits_state(state_r), %ebx

#undef strm_r
#undef state_r

#define in_r       %esi
#define from_r     %esi
#define out_r      %edi

        movl    in(%esp), in_r
        movl    last(%esp), %ecx
        cmpl    in_r, %ecx
        ja      .L_align_long           /* if in < last */

        addl    $11, %ecx               /* ecx = &in[ avail_in ] */
        subl    in_r, %ecx              /* ecx = avail_in */
        movl    $12, %eax
        subl    %ecx, %eax              /* eax = 12 - avail_in */
        leal    buf(%esp), %edi
        rep     movsb                   /* memcpy( buf, in, avail_in ) */
        movl    %eax, %ecx
        xorl    %eax, %eax
        rep     stosb         /* memset( &buf[ avail_in ], 0, 12 - avail_in ) */
        leal    buf(%esp), in_r         /* in = buf */
        movl    in_r, last(%esp)        /* last = in, do just one iteration */
        jmp     .L_is_aligned

        /* align in_r on long boundary */
.L_align_long:
        testl   $3, in_r
        jz      .L_is_aligned
        xorl    %eax, %eax
        movb    (in_r), %al
        incl    in_r
        movl    %ebx, %ecx
        addl    $8, %ebx
        shll    %cl, %eax
        orl     %eax, %ebp
        jmp     .L_align_long

.L_is_aligned:
        movl    out(%esp), out_r

#if defined( NO_MMX )
        jmp     .L_do_loop
#endif

#if defined( USE_MMX )
        jmp     .L_init_mmx
#endif

/*** Runtime MMX check ***/

#if defined( RUN_TIME_MMX )
.L_check_mmx:
        cmpl    $DO_USE_MMX, inflate_fast_use_mmx
        je      .L_init_mmx
        ja      .L_do_loop /* > 2 */

        pushl   %eax
        pushl   %ebx
        pushl   %ecx
        pushl   %edx
        pushf
        movl    (%esp), %eax      /* copy eflags to eax */
        xorl    $0x200000, (%esp) /* try toggling ID bit of eflags (bit 21)
                                   * to see if cpu supports cpuid...
                                   * ID bit method not supported by NexGen but
                                   * bios may load a cpuid instruction and
                                   * cpuid may be disabled on Cyrix 5-6x86 */
        popf
        pushf
        popl    %edx              /* copy new eflags to edx */
        xorl    %eax, %edx        /* test if ID bit is flipped */
        jz      .L_dont_use_mmx   /* not flipped if zero */
        xorl    %eax, %eax
        cpuid
        cmpl    $0x756e6547, %ebx /* check for GenuineIntel in ebx,ecx,edx */
        jne     .L_dont_use_mmx
        cmpl    $0x6c65746e, %ecx
        jne     .L_dont_use_mmx
        cmpl    $0x49656e69, %edx
        jne     .L_dont_use_mmx
        movl    $1, %eax
        cpuid                     /* get cpu features */
        shrl    $8, %eax
        andl    $15, %eax
        cmpl    $6, %eax          /* check for Pentium family, is 0xf for P4 */
        jne     .L_dont_use_mmx
        testl   $0x800000, %edx   /* test if MMX feature is set (bit 23) */
        jnz     .L_use_mmx
        jmp     .L_dont_use_mmx
.L_use_mmx:
        movl    $DO_USE_MMX, inflate_fast_use_mmx
        jmp     .L_check_mmx_pop
.L_dont_use_mmx:
        movl    $DONT_USE_MMX, inflate_fast_use_mmx
.L_check_mmx_pop:
        popl    %edx
        popl    %ecx
        popl    %ebx
        popl    %eax
        jmp     .L_check_mmx
#endif


/*** Non-MMX code ***/

#if defined ( NO_MMX ) || defined( RUN_TIME_MMX )

#define hold_r     %ebp
#define bits_r     %bl
#define bitslong_r %ebx

.align 32,0x90
.L_while_test:
        /* while (in < last && out < end)
         */
        cmpl    out_r, end(%esp)
        jbe     .L_break_loop           /* if (out >= end) */

        cmpl    in_r, last(%esp)
        jbe     .L_break_loop

.L_do_loop:
        /* regs: %esi = in, %ebp = hold, %bl = bits, %edi = out
         *
         * do {
         *   if (bits < 15) {
         *     hold |= *((unsigned short *)in)++ << bits;
         *     bits += 16
         *   }
         *   this = lcode[hold & lmask]
         */
        cmpb    $15, bits_r
        ja      .L_get_length_code      /* if (15 < bits) */

        xorl    %eax, %eax
        lodsw                           /* al = *(ushort *)in++ */
        movb    bits_r, %cl             /* cl = bits, needs it for shifting */
        addb    $16, bits_r             /* bits += 16 */
        shll    %cl, %eax
        orl     %eax, hold_r            /* hold |= *((ushort *)in)++ << bits */

.L_get_length_code:
        movl    lmask(%esp), %edx       /* edx = lmask */
        movl    lcode(%esp), %ecx       /* ecx = lcode */
        andl    hold_r, %edx            /* edx &= hold */
        movl    (%ecx,%edx,4), %eax     /* eax = lcode[hold & lmask] */

.L_dolen:
        /* regs: %esi = in, %ebp = hold, %bl = bits, %edi = out
         *
         * dolen:
         *    bits -= this.bits;
         *    hold >>= this.bits
         */
        movb    %ah, %cl                /* cl = this.bits */
        subb    %ah, bits_r             /* bits -= this.bits */
        shrl    %cl, hold_r             /* hold >>= this.bits */

        /* check if op is a literal
         * if (op == 0) {
         *    PUP(out) = this.val;
         *  }
         */
        testb   %al, %al
        jnz     .L_test_for_length_base /* if (op != 0) 45.7% */

        shrl    $16, %eax               /* output this.val char */
        stosb
        jmp     .L_while_test

.L_test_for_length_base:
        /* regs: %esi = in, %ebp = hold, %bl = bits, %edi = out, %edx = len
         *
         * else if (op & 16) {
         *   len = this.val
         *   op &= 15
         *   if (op) {
         *     if (op > bits) {
         *       hold |= *((unsigned short *)in)++ << bits;
         *       bits += 16
         *     }
         *     len += hold & mask[op];
         *     bits -= op;
         *     hold >>= op;
         *   }
         */
#define len_r %edx
        movl    %eax, len_r             /* len = this */
        shrl    $16, len_r              /* len = this.val */
        movb    %al, %cl

        testb   $16, %al
        jz      .L_test_for_second_level_length /* if ((op & 16) == 0) 8% */
        andb    $15, %cl                /* op &= 15 */
        jz      .L_save_len             /* if (!op) */
        cmpb    %cl, bits_r
        jae     .L_add_bits_to_len      /* if (op <= bits) */

        movb    %cl, %ch                /* stash op in ch, freeing cl */
        xorl    %eax, %eax
        lodsw                           /* al = *(ushort *)in++ */
        movb    bits_r, %cl             /* cl = bits, needs it for shifting */
        addb    $16, bits_r             /* bits += 16 */
        shll    %cl, %eax
        orl     %eax, hold_r            /* hold |= *((ushort *)in)++ << bits */
        movb    %ch, %cl                /* move op back to ecx */

.L_add_bits_to_len:
        movl    $1, %eax
        shll    %cl, %eax
        decl    %eax
        subb    %cl, bits_r
        andl    hold_r, %eax            /* eax &= hold */
        shrl    %cl, hold_r
        addl    %eax, len_r             /* len += hold & mask[op] */

.L_save_len:
        movl    len_r, len(%esp)        /* save len */
#undef  len_r

.L_decode_distance:
        /* regs: %esi = in, %ebp = hold, %bl = bits, %edi = out, %edx = dist
         *
         *   if (bits < 15) {
         *     hold |= *((unsigned short *)in)++ << bits;
         *     bits += 16
         *   }
         *   this = dcode[hold & dmask];
         * dodist:
         *   bits -= this.bits;
         *   hold >>= this.bits;
         *   op = this.op;
         */

        cmpb    $15, bits_r
        ja      .L_get_distance_code    /* if (15 < bits) */

        xorl    %eax, %eax
        lodsw                           /* al = *(ushort *)in++ */
        movb    bits_r, %cl             /* cl = bits, needs it for shifting */
        addb    $16, bits_r             /* bits += 16 */
        shll    %cl, %eax
        orl     %eax, hold_r            /* hold |= *((ushort *)in)++ << bits */

.L_get_distance_code:
        movl    dmask(%esp), %edx       /* edx = dmask */
        movl    dcode(%esp), %ecx       /* ecx = dcode */
        andl    hold_r, %edx            /* edx &= hold */
        movl    (%ecx,%edx,4), %eax     /* eax = dcode[hold & dmask] */

#define dist_r %edx
.L_dodist:
        movl    %eax, dist_r            /* dist = this */
        shrl    $16, dist_r             /* dist = this.val */
        movb    %ah, %cl
        subb    %ah, bits_r             /* bits -= this.bits */
        shrl    %cl, hold_r             /* hold >>= this.bits */

        /* if (op & 16) {
         *   dist = this.val
         *   op &= 15
         *   if (op > bits) {
         *     hold |= *((unsigned short *)in)++ << bits;
         *     bits += 16
         *   }
         *   dist += hold & mask[op];
         *   bits -= op;
         *   hold >>= op;
         */
        movb    %al, %cl                /* cl = this.op */

        testb   $16, %al                /* if ((op & 16) == 0) */
        jz      .L_test_for_second_level_dist
        andb    $15, %cl                /* op &= 15 */
        jz      .L_check_dist_one
        cmpb    %cl, bits_r
        jae     .L_add_bits_to_dist     /* if (op <= bits) 97.6% */

        movb    %cl, %ch                /* stash op in ch, freeing cl */
        xorl    %eax, %eax
        lodsw                           /* al = *(ushort *)in++ */
        movb    bits_r, %cl             /* cl = bits, needs it for shifting */
        addb    $16, bits_r             /* bits += 16 */
        shll    %cl, %eax
        orl     %eax, hold_r            /* hold |= *((ushort *)in)++ << bits */
        movb    %ch, %cl                /* move op back to ecx */

.L_add_bits_to_dist:
        movl    $1, %eax
        shll    %cl, %eax
        decl    %eax                    /* (1 << op) - 1 */
        subb    %cl, bits_r
        andl    hold_r, %eax            /* eax &= hold */
        shrl    %cl, hold_r
        addl    %eax, dist_r            /* dist += hold & ((1 << op) - 1) */
        jmp     .L_check_window

.L_check_window:
        /* regs: %esi = from, %ebp = hold, %bl = bits, %edi = out, %edx = dist
         *       %ecx = nbytes
         *
         * nbytes = out - beg;
         * if (dist <= nbytes) {
         *   from = out - dist;
         *   do {
         *     PUP(out) = PUP(from);
         *   } while (--len > 0) {
         * }
         */

        movl    in_r, in(%esp)          /* save in so from can use it's reg */
        movl    out_r, %eax
        subl    beg(%esp), %eax         /* nbytes = out - beg */

        cmpl    dist_r, %eax
        jb      .L_clip_window          /* if (dist > nbytes) 4.2% */

        movl    len(%esp), %ecx
        movl    out_r, from_r
        subl    dist_r, from_r          /* from = out - dist */

        subl    $3, %ecx
        movb    (from_r), %al
        movb    %al, (out_r)
        movb    1(from_r), %al
        movb    2(from_r), %dl
        addl    $3, from_r
        movb    %al, 1(out_r)
        movb    %dl, 2(out_r)
        addl    $3, out_r
        rep     movsb

        movl    in(%esp), in_r          /* move in back to %esi, toss from */
        jmp     .L_while_test

.align 16,0x90
.L_check_dist_one:
        cmpl    $1, dist_r
        jne     .L_check_window
        cmpl    out_r, beg(%esp)
        je      .L_check_window

        decl    out_r
        movl    len(%esp), %ecx
        movb    (out_r), %al
        subl    $3, %ecx

        movb    %al, 1(out_r)
        movb    %al, 2(out_r)
        movb    %al, 3(out_r)
        addl    $4, out_r
        rep     stosb

        jmp     .L_while_test

.align 16,0x90
.L_test_for_second_level_length:
        /* else if ((op & 64) == 0) {
         *   this = lcode[this.val + (hold & mask[op])];
         * }
         */
        testb   $64, %al
        jnz     .L_test_for_end_of_block  /* if ((op & 64) != 0) */

        movl    $1, %eax
        shll    %cl, %eax
        decl    %eax
        andl    hold_r, %eax            /* eax &= hold */
        addl    %edx, %eax              /* eax += this.val */
        movl    lcode(%esp), %edx       /* edx = lcode */
        movl    (%edx,%eax,4), %eax     /* eax = lcode[val + (hold&mask[op])] */
        jmp     .L_dolen

.align 16,0x90
.L_test_for_second_level_dist:
        /* else if ((op & 64) == 0) {
         *   this = dcode[this.val + (hold & mask[op])];
         * }
         */
        testb   $64, %al
        jnz     .L_invalid_distance_code  /* if ((op & 64) != 0) */

        movl    $1, %eax
        shll    %cl, %eax
        decl    %eax
        andl    hold_r, %eax            /* eax &= hold */
        addl    %edx, %eax              /* eax += this.val */
        movl    dcode(%esp), %edx       /* edx = dcode */
        movl    (%edx,%eax,4), %eax     /* eax = dcode[val + (hold&mask[op])] */
        jmp     .L_dodist

.align 16,0x90
.L_clip_window:
        /* regs: %esi = from, %ebp = hold, %bl = bits, %edi = out, %edx = dist
         *       %ecx = nbytes
         *
         * else {
         *   if (dist > wsize) {
         *     invalid distance
         *   }
         *   from = window;
         *   nbytes = dist - nbytes;
         *   if (write == 0) {
         *     from += wsize - nbytes;
         */
#define nbytes_r %ecx
        movl    %eax, nbytes_r
        movl    wsize(%esp), %eax       /* prepare for dist compare */
        negl    nbytes_r                /* nbytes = -nbytes */
        movl    window(%esp), from_r    /* from = window */

        cmpl    dist_r, %eax
        jb      .L_invalid_distance_too_far /* if (dist > wsize) */

        addl    dist_r, nbytes_r        /* nbytes = dist - nbytes */
        cmpl    $0, write(%esp)
        jne     .L_wrap_around_window   /* if (write != 0) */

        subl    nbytes_r, %eax
        addl    %eax, from_r            /* from += wsize - nbytes */

        /* regs: %esi = from, %ebp = hold, %bl = bits, %edi = out, %edx = dist
         *       %ecx = nbytes, %eax = len
         *
         *     if (nbytes < len) {
         *       len -= nbytes;
         *       do {
         *         PUP(out) = PUP(from);
         *       } while (--nbytes);
         *       from = out - dist;
         *     }
         *   }
         */
#define len_r %eax
        movl    len(%esp), len_r
        cmpl    nbytes_r, len_r
        jbe     .L_do_copy1             /* if (nbytes >= len) */

        subl    nbytes_r, len_r         /* len -= nbytes */
        rep     movsb
        movl    out_r, from_r
        subl    dist_r, from_r          /* from = out - dist */
        jmp     .L_do_copy1

        cmpl    nbytes_r, len_r
        jbe     .L_do_copy1             /* if (nbytes >= len) */

        subl    nbytes_r, len_r         /* len -= nbytes */
        rep     movsb
        movl    out_r, from_r
        subl    dist_r, from_r          /* from = out - dist */
        jmp     .L_do_copy1

.L_wrap_around_window:
        /* regs: %esi = from, %ebp = hold, %bl = bits, %edi = out, %edx = dist
         *       %ecx = nbytes, %eax = write, %eax = len
         *
         *   else if (write < nbytes) {
         *     from += wsize + write - nbytes;
         *     nbytes -= write;
         *     if (nbytes < len) {
         *       len -= nbytes;
         *       do {
         *         PUP(out) = PUP(from);
         *       } while (--nbytes);
         *       from = window;
         *       nbytes = write;
         *       if (nbytes < len) {
         *         len -= nbytes;
         *         do {
         *           PUP(out) = PUP(from);
         *         } while(--nbytes);
         *         from = out - dist;
         *       }
         *     }
         *   }
         */
#define write_r %eax
        movl    write(%esp), write_r
        cmpl    write_r, nbytes_r
        jbe     .L_contiguous_in_window /* if (write >= nbytes) */

        addl    wsize(%esp), from_r
        addl    write_r, from_r
        subl    nbytes_r, from_r        /* from += wsize + write - nbytes */
        subl    write_r, nbytes_r       /* nbytes -= write */
#undef write_r

        movl    len(%esp), len_r
        cmpl    nbytes_r, len_r
        jbe     .L_do_copy1             /* if (nbytes >= len) */

        subl    nbytes_r, len_r         /* len -= nbytes */
        rep     movsb
        movl    window(%esp), from_r    /* from = window */
        movl    write(%esp), nbytes_r   /* nbytes = write */
        cmpl    nbytes_r, len_r
        jbe     .L_do_copy1             /* if (nbytes >= len) */

        subl    nbytes_r, len_r         /* len -= nbytes */
        rep     movsb
        movl    out_r, from_r
        subl    dist_r, from_r          /* from = out - dist */
        jmp     .L_do_copy1

.L_contiguous_in_window:
        /* regs: %esi = from, %ebp = hold, %bl = bits, %edi = out, %edx = dist
         *       %ecx = nbytes, %eax = write, %eax = len
         *
         *   else {
         *     from += write - nbytes;
         *     if (nbytes < len) {
         *       len -= nbytes;
         *       do {
         *         PUP(out) = PUP(from);
         *       } while (--nbytes);
         *       from = out - dist;
         *     }
         *   }
         */
#define write_r %eax
        addl    write_r, from_r
        subl    nbytes_r, from_r        /* from += write - nbytes */
#undef write_r

        movl    len(%esp), len_r
        cmpl    nbytes_r, len_r
        jbe     .L_do_copy1             /* if (nbytes >= len) */

        subl    nbytes_r, len_r         /* len -= nbytes */
        rep     movsb
        movl    out_r, from_r
        subl    dist_r, from_r          /* from = out - dist */

.L_do_copy1:
        /* regs: %esi = from, %esi = in, %ebp = hold, %bl = bits, %edi = out
         *       %eax = len
         *
         *     while (len > 0) {
         *       PUP(out) = PUP(from);
         *       len--;
         *     }
         *   }
         * } while (in < last && out < end);
         */
#undef nbytes_r
#define in_r %esi
        movl    len_r, %ecx
        rep     movsb

        movl    in(%esp), in_r          /* move in back to %esi, toss from */
        jmp     .L_while_test

#undef len_r
#undef dist_r

#endif /* NO_MMX || RUN_TIME_MMX */


/*** MMX code ***/

#if defined( USE_MMX ) || defined( RUN_TIME_MMX )

.align 32,0x90
.L_init_mmx:
        emms

#undef  bits_r
#undef  bitslong_r
#define bitslong_r %ebp
#define hold_mm    %mm0
        movd    %ebp, hold_mm
        movl    %ebx, bitslong_r

#define used_mm   %mm1
#define dmask2_mm %mm2
#define lmask2_mm %mm3
#define lmask_mm  %mm4
#define dmask_mm  %mm5
#define tmp_mm    %mm6

        movd    lmask(%esp), lmask_mm
        movq    lmask_mm, lmask2_mm
        movd    dmask(%esp), dmask_mm
        movq    dmask_mm, dmask2_mm
        pxor    used_mm, used_mm
        movl    lcode(%esp), %ebx       /* ebx = lcode */
        jmp     .L_do_loop_mmx

.align 32,0x90
.L_while_test_mmx:
        /* while (in < last && out < end)
         */
        cmpl    out_r, end(%esp)
        jbe     .L_break_loop           /* if (out >= end) */

        cmpl    in_r, last(%esp)
        jbe     .L_break_loop

.L_do_loop_mmx:
        psrlq   used_mm, hold_mm        /* hold_mm >>= last bit length */

        cmpl    $32, bitslong_r
        ja      .L_get_length_code_mmx  /* if (32 < bits) */

        movd    bitslong_r, tmp_mm
        movd    (in_r), %mm7
        addl    $4, in_r
        psllq   tmp_mm, %mm7
        addl    $32, bitslong_r
        por     %mm7, hold_mm           /* hold_mm |= *((uint *)in)++ << bits */

.L_get_length_code_mmx:
        pand    hold_mm, lmask_mm
        movd    lmask_mm, %eax
        movq    lmask2_mm, lmask_mm
        movl    (%ebx,%eax,4), %eax     /* eax = lcode[hold & lmask] */

.L_dolen_mmx:
        movzbl  %ah, %ecx               /* ecx = this.bits */
        movd    %ecx, used_mm
        subl    %ecx, bitslong_r        /* bits -= this.bits */

        testb   %al, %al
        jnz     .L_test_for_length_base_mmx /* if (op != 0) 45.7% */

        shrl    $16, %eax               /* output this.val char */
        stosb
        jmp     .L_while_test_mmx

.L_test_for_length_base_mmx:
#define len_r  %edx
        movl    %eax, len_r             /* len = this */
        shrl    $16, len_r              /* len = this.val */

        testb   $16, %al
        jz      .L_test_for_second_level_length_mmx /* if ((op & 16) == 0) 8% */
        andl    $15, %eax               /* op &= 15 */
        jz      .L_decode_distance_mmx  /* if (!op) */

        psrlq   used_mm, hold_mm        /* hold_mm >>= last bit length */
        movd    %eax, used_mm
        movd    hold_mm, %ecx
        subl    %eax, bitslong_r
        andl    .L_mask(,%eax,4), %ecx
        addl    %ecx, len_r             /* len += hold & mask[op] */

.L_decode_distance_mmx:
        psrlq   used_mm, hold_mm        /* hold_mm >>= last bit length */

        cmpl    $32, bitslong_r
        ja      .L_get_dist_code_mmx    /* if (32 < bits) */

        movd    bitslong_r, tmp_mm
        movd    (in_r), %mm7
        addl    $4, in_r
        psllq   tmp_mm, %mm7
        addl    $32, bitslong_r
        por     %mm7, hold_mm           /* hold_mm |= *((uint *)in)++ << bits */

.L_get_dist_code_mmx:
        movl    dcode(%esp), %ebx       /* ebx = dcode */
        pand    hold_mm, dmask_mm
        movd    dmask_mm, %eax
        movq    dmask2_mm, dmask_mm
        movl    (%ebx,%eax,4), %eax     /* eax = dcode[hold & lmask] */

.L_dodist_mmx:
#define dist_r %ebx
        movzbl  %ah, %ecx               /* ecx = this.bits */
        movl    %eax, dist_r
        shrl    $16, dist_r             /* dist  = this.val */
        subl    %ecx, bitslong_r        /* bits -= this.bits */
        movd    %ecx, used_mm

        testb   $16, %al                /* if ((op & 16) == 0) */
        jz      .L_test_for_second_level_dist_mmx
        andl    $15, %eax               /* op &= 15 */
        jz      .L_check_dist_one_mmx

.L_add_bits_to_dist_mmx:
        psrlq   used_mm, hold_mm        /* hold_mm >>= last bit length */
        movd    %eax, used_mm           /* save bit length of current op */
        movd    hold_mm, %ecx           /* get the next bits on input stream */
        subl    %eax, bitslong_r        /* bits -= op bits */
        andl    .L_mask(,%eax,4), %ecx  /* ecx   = hold & mask[op] */
        addl    %ecx, dist_r            /* dist += hold & mask[op] */

.L_check_window_mmx:
        movl    in_r, in(%esp)          /* save in so from can use it's reg */
        movl    out_r, %eax
        subl    beg(%esp), %eax         /* nbytes = out - beg */

        cmpl    dist_r, %eax
        jb      .L_clip_window_mmx      /* if (dist > nbytes) 4.2% */

        movl    len_r, %ecx
        movl    out_r, from_r
        subl    dist_r, from_r          /* from = out - dist */

        subl    $3, %ecx
        movb    (from_r), %al
        movb    %al, (out_r)
        movb    1(from_r), %al
        movb    2(from_r), %dl
        addl    $3, from_r
        movb    %al, 1(out_r)
        movb    %dl, 2(out_r)
        addl    $3, out_r
        rep     movsb

        movl    in(%esp), in_r          /* move in back to %esi, toss from */
        movl    lcode(%esp), %ebx       /* move lcode back to %ebx, toss dist */
        jmp     .L_while_test_mmx

.align 16,0x90
.L_check_dist_one_mmx:
        cmpl    $1, dist_r
        jne     .L_check_window_mmx
        cmpl    out_r, beg(%esp)
        je      .L_check_window_mmx

        decl    out_r
        movl    len_r, %ecx
        movb    (out_r), %al
        subl    $3, %ecx

        movb    %al, 1(out_r)
        movb    %al, 2(out_r)
        movb    %al, 3(out_r)
        addl    $4, out_r
        rep     stosb

        movl    lcode(%esp), %ebx       /* move lcode back to %ebx, toss dist */
        jmp     .L_while_test_mmx

.align 16,0x90
.L_test_for_second_level_length_mmx:
        testb   $64, %al
        jnz     .L_test_for_end_of_block  /* if ((op & 64) != 0) */

        andl    $15, %eax
        psrlq   used_mm, hold_mm        /* hold_mm >>= last bit length */
        movd    hold_mm, %ecx
        andl    .L_mask(,%eax,4), %ecx
        addl    len_r, %ecx
        movl    (%ebx,%ecx,4), %eax     /* eax = lcode[hold & lmask] */
        jmp     .L_dolen_mmx

.align 16,0x90
.L_test_for_second_level_dist_mmx:
        testb   $64, %al
        jnz     .L_invalid_distance_code  /* if ((op & 64) != 0) */

        andl    $15, %eax
        psrlq   used_mm, hold_mm        /* hold_mm >>= last bit length */
        movd    hold_mm, %ecx
        andl    .L_mask(,%eax,4), %ecx
        movl    dcode(%esp), %eax       /* ecx = dcode */
        addl    dist_r, %ecx
        movl    (%eax,%ecx,4), %eax     /* eax = lcode[hold & lmask] */
        jmp     .L_dodist_mmx

.align 16,0x90
.L_clip_window_mmx:
#define nbytes_r %ecx
        movl    %eax, nbytes_r
        movl    wsize(%esp), %eax       /* prepare for dist compare */
        negl    nbytes_r                /* nbytes = -nbytes */
        movl    window(%esp), from_r    /* from = window */

        cmpl    dist_r, %eax
        jb      .L_invalid_distance_too_far /* if (dist > wsize) */

        addl    dist_r, nbytes_r        /* nbytes = dist - nbytes */
        cmpl    $0, write(%esp)
        jne     .L_wrap_around_window_mmx /* if (write != 0) */

        subl    nbytes_r, %eax
        addl    %eax, from_r            /* from += wsize - nbytes */

        cmpl    nbytes_r, len_r
        jbe     .L_do_copy1_mmx         /* if (nbytes >= len) */

        subl    nbytes_r, len_r         /* len -= nbytes */
        rep     movsb
        movl    out_r, from_r
        subl    dist_r, from_r          /* from = out - dist */
        jmp     .L_do_copy1_mmx

        cmpl    nbytes_r, len_r
        jbe     .L_do_copy1_mmx         /* if (nbytes >= len) */

        subl    nbytes_r, len_r         /* len -= nbytes */
        rep     movsb
        movl    out_r, from_r
        subl    dist_r, from_r          /* from = out - dist */
        jmp     .L_do_copy1_mmx

.L_wrap_around_window_mmx:
#define write_r %eax
        movl    write(%esp), write_r
        cmpl    write_r, nbytes_r
        jbe     .L_contiguous_in_window_mmx /* if (write >= nbytes) */

        addl    wsize(%esp), from_r
        addl    write_r, from_r
        subl    nbytes_r, from_r        /* from += wsize + write - nbytes */
        subl    write_r, nbytes_r       /* nbytes -= write */
#undef write_r

        cmpl    nbytes_r, len_r
        jbe     .L_do_copy1_mmx         /* if (nbytes >= len) */

        subl    nbytes_r, len_r         /* len -= nbytes */
        rep     movsb
        movl    window(%esp), from_r    /* from = window */
        movl    write(%esp), nbytes_r   /* nbytes = write */
        cmpl    nbytes_r, len_r
        jbe     .L_do_copy1_mmx         /* if (nbytes >= len) */

        subl    nbytes_r, len_r         /* len -= nbytes */
        rep     movsb
        movl    out_r, from_r
        subl    dist_r, from_r          /* from = out - dist */
        jmp     .L_do_copy1_mmx

.L_contiguous_in_window_mmx:
#define write_r %eax
        addl    write_r, from_r
        subl    nbytes_r, from_r        /* from += write - nbytes */
#undef write_r

        cmpl    nbytes_r, len_r
        jbe     .L_do_copy1_mmx         /* if (nbytes >= len) */

        subl    nbytes_r, len_r         /* len -= nbytes */
        rep     movsb
        movl    out_r, from_r
        subl    dist_r, from_r          /* from = out - dist */

.L_do_copy1_mmx:
#undef nbytes_r
#define in_r %esi
        movl    len_r, %ecx
        rep     movsb

        movl    in(%esp), in_r          /* move in back to %esi, toss from */
        movl    lcode(%esp), %ebx       /* move lcode back to %ebx, toss dist */
        jmp     .L_while_test_mmx

#undef hold_r
#undef bitslong_r

#endif /* USE_MMX || RUN_TIME_MMX */


/*** USE_MMX, NO_MMX, and RUNTIME_MMX from here on ***/

.L_invalid_distance_code:
        /* else {
         *   strm->msg = "invalid distance code";
         *   state->mode = BAD;
         * }
         */
        movl    $.L_invalid_distance_code_msg, %ecx
        movl    $INFLATE_MODE_BAD, %edx
        jmp     .L_update_stream_state

.L_test_for_end_of_block:
        /* else if (op & 32) {
         *   state->mode = TYPE;
         *   break;
         * }
         */
        testb   $32, %al
        jz      .L_invalid_literal_length_code  /* if ((op & 32) == 0) */

        movl    $0, %ecx
        movl    $INFLATE_MODE_TYPE, %edx
        jmp     .L_update_stream_state

.L_invalid_literal_length_code:
        /* else {
         *   strm->msg = "invalid literal/length code";
         *   state->mode = BAD;
         * }
         */
        movl    $.L_invalid_literal_length_code_msg, %ecx
        movl    $INFLATE_MODE_BAD, %edx
        jmp     .L_update_stream_state

.L_invalid_distance_too_far:
        /* strm->msg = "invalid distance too far back";
         * state->mode = BAD;
         */
        movl    in(%esp), in_r          /* from_r has in's reg, put in back */
        movl    $.L_invalid_distance_too_far_msg, %ecx
        movl    $INFLATE_MODE_BAD, %edx
        jmp     .L_update_stream_state

.L_update_stream_state:
        /* set strm->msg = %ecx, strm->state->mode = %edx */
        movl    strm_sp(%esp), %eax
        testl   %ecx, %ecx              /* if (msg != NULL) */
        jz      .L_skip_msg
        movl    %ecx, msg_strm(%eax)    /* strm->msg = msg */
.L_skip_msg:
        movl    state_strm(%eax), %eax  /* state = strm->state */
        movl    %edx, mode_state(%eax)  /* state->mode = edx (BAD | TYPE) */
        jmp     .L_break_loop

.align 32,0x90
.L_break_loop:

/*
 * Regs:
 *
 * bits = %ebp when mmx, and in %ebx when non-mmx
 * hold = %hold_mm when mmx, and in %ebp when non-mmx
 * in   = %esi
 * out  = %edi
 */

#if defined( USE_MMX ) || defined( RUN_TIME_MMX )

#if defined( RUN_TIME_MMX )

        cmpl    $DO_USE_MMX, inflate_fast_use_mmx
        jne     .L_update_next_in

#endif /* RUN_TIME_MMX */

        movl    %ebp, %ebx

.L_update_next_in:

#endif

#define strm_r  %eax
#define state_r %edx

        /* len = bits >> 3;
         * in -= len;
         * bits -= len << 3;
         * hold &= (1U << bits) - 1;
         * state->hold = hold;
         * state->bits = bits;
         * strm->next_in = in;
         * strm->next_out = out;
         */
        movl    strm_sp(%esp), strm_r
        movl    %ebx, %ecx
        movl    state_strm(strm_r), state_r
        shrl    $3, %ecx
        subl    %ecx, in_r
        shll    $3, %ecx
        subl    %ecx, %ebx
        movl    out_r, next_out_strm(strm_r)
        movl    %ebx, bits_state(state_r)
        movl    %ebx, %ecx

        leal    buf(%esp), %ebx
        cmpl    %ebx, last(%esp)
        jne     .L_buf_not_used         /* if buf != last */

        subl    %ebx, in_r              /* in -= buf */
        movl    next_in_strm(strm_r), %ebx
        movl    %ebx, last(%esp)        /* last = strm->next_in */
        addl    %ebx, in_r              /* in += strm->next_in */
        movl    avail_in_strm(strm_r), %ebx
        subl    $11, %ebx
        addl    %ebx, last(%esp)    /* last = &strm->next_in[ avail_in - 11 ] */

.L_buf_not_used:
        movl    in_r, next_in_strm(strm_r)

        movl    $1, %ebx
        shll    %cl, %ebx
        decl    %ebx

#if defined( USE_MMX ) || defined( RUN_TIME_MMX )

#if defined( RUN_TIME_MMX )

        cmpl    $DO_USE_MMX, inflate_fast_use_mmx
        jne     .L_update_hold

#endif /* RUN_TIME_MMX */

        psrlq   used_mm, hold_mm        /* hold_mm >>= last bit length */
        movd    hold_mm, %ebp

        emms

.L_update_hold:

#endif /* USE_MMX || RUN_TIME_MMX */

        andl    %ebx, %ebp
        movl    %ebp, hold_state(state_r)

#define last_r %ebx

        /* strm->avail_in = in < last ? 11 + (last - in) : 11 - (in - last) */
        movl    last(%esp), last_r
        cmpl    in_r, last_r
        jbe     .L_last_is_smaller     /* if (in >= last) */

        subl    in_r, last_r           /* last -= in */
        addl    $11, last_r            /* last += 11 */
        movl    last_r, avail_in_strm(strm_r)
        jmp     .L_fixup_out
.L_last_is_smaller:
        subl    last_r, in_r           /* in -= last */
        negl    in_r                   /* in = -in */
        addl    $11, in_r              /* in += 11 */
        movl    in_r, avail_in_strm(strm_r)

#undef last_r
#define end_r %ebx

.L_fixup_out:
        /* strm->avail_out = out < end ? 257 + (end - out) : 257 - (out - end)*/
        movl    end(%esp), end_r
        cmpl    out_r, end_r
        jbe     .L_end_is_smaller      /* if (out >= end) */

        subl    out_r, end_r           /* end -= out */
        addl    $257, end_r            /* end += 257 */
        movl    end_r, avail_out_strm(strm_r)
        jmp     .L_done
.L_end_is_smaller:
        subl    end_r, out_r           /* out -= end */
        negl    out_r                  /* out = -out */
        addl    $257, out_r            /* out += 257 */
        movl    out_r, avail_out_strm(strm_r)

#undef end_r
#undef strm_r
#undef state_r

.L_done:
        addl    $local_var_size, %esp
        popf
        popl    %ebx
        popl    %ebp
        popl    %esi
        popl    %edi
        ret

#if defined( GAS_ELF )
/* elf info */
.type inflate_fast,@function
.size inflate_fast,.-inflate_fast
#endif
