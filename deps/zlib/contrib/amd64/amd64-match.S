/*
 * match.S -- optimized version of longest_match()
 * based on the similar work by Gilles Vollant, and Brian Raiter, written 1998
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the BSD License. Use by owners of Che Guevarra
 * parafernalia is prohibited, where possible, and highly discouraged
 * elsewhere.
 */

#ifndef NO_UNDERLINE
#	define	match_init	_match_init
#	define	longest_match	_longest_match
#endif

#define	scanend		ebx
#define	scanendw	bx
#define	chainlenwmask	edx /* high word: current chain len low word: s->wmask */
#define	curmatch	rsi
#define	curmatchd	esi
#define	windowbestlen	r8
#define	scanalign	r9
#define	scanalignd	r9d
#define	window		r10
#define	bestlen		r11
#define	bestlend	r11d
#define	scanstart	r12d
#define	scanstartw	r12w
#define scan		r13
#define nicematch	r14d
#define	limit		r15
#define	limitd		r15d
#define prev		rcx

/*
 * The 258 is a "magic number, not a parameter -- changing it
 * breaks the hell loose
 */
#define	MAX_MATCH	(258)
#define	MIN_MATCH	(3)
#define	MIN_LOOKAHEAD	(MAX_MATCH + MIN_MATCH + 1)
#define	MAX_MATCH_8	((MAX_MATCH + 7) & ~7)

/* stack frame offsets */
#define	LocalVarsSize	(112)
#define _chainlenwmask	( 8-LocalVarsSize)(%rsp)
#define _windowbestlen	(16-LocalVarsSize)(%rsp)
#define save_r14        (24-LocalVarsSize)(%rsp)
#define save_rsi        (32-LocalVarsSize)(%rsp)
#define save_rbx        (40-LocalVarsSize)(%rsp)
#define save_r12        (56-LocalVarsSize)(%rsp)
#define save_r13        (64-LocalVarsSize)(%rsp)
#define save_r15        (80-LocalVarsSize)(%rsp)


.globl	match_init, longest_match

/*
 * On AMD64 the first argument of a function (in our case -- the pointer to
 * deflate_state structure) is passed in %rdi, hence our offsets below are
 * all off of that.
 */

/* you can check the structure offset by running

#include <stdlib.h>
#include <stdio.h>
#include "deflate.h"

void print_depl()
{
deflate_state ds;
deflate_state *s=&ds;
printf("size pointer=%u\n",(int)sizeof(void*));

printf("#define dsWSize         (%3u)(%%rdi)\n",(int)(((char*)&(s->w_size))-((char*)s)));
printf("#define dsWMask         (%3u)(%%rdi)\n",(int)(((char*)&(s->w_mask))-((char*)s)));
printf("#define dsWindow        (%3u)(%%rdi)\n",(int)(((char*)&(s->window))-((char*)s)));
printf("#define dsPrev          (%3u)(%%rdi)\n",(int)(((char*)&(s->prev))-((char*)s)));
printf("#define dsMatchLen      (%3u)(%%rdi)\n",(int)(((char*)&(s->match_length))-((char*)s)));
printf("#define dsPrevMatch     (%3u)(%%rdi)\n",(int)(((char*)&(s->prev_match))-((char*)s)));
printf("#define dsStrStart      (%3u)(%%rdi)\n",(int)(((char*)&(s->strstart))-((char*)s)));
printf("#define dsMatchStart    (%3u)(%%rdi)\n",(int)(((char*)&(s->match_start))-((char*)s)));
printf("#define dsLookahead     (%3u)(%%rdi)\n",(int)(((char*)&(s->lookahead))-((char*)s)));
printf("#define dsPrevLen       (%3u)(%%rdi)\n",(int)(((char*)&(s->prev_length))-((char*)s)));
printf("#define dsMaxChainLen   (%3u)(%%rdi)\n",(int)(((char*)&(s->max_chain_length))-((char*)s)));
printf("#define dsGoodMatch     (%3u)(%%rdi)\n",(int)(((char*)&(s->good_match))-((char*)s)));
printf("#define dsNiceMatch     (%3u)(%%rdi)\n",(int)(((char*)&(s->nice_match))-((char*)s)));
}

*/


/*
  to compile for XCode 3.2 on MacOSX x86_64
  - run "gcc -g -c -DXCODE_MAC_X64_STRUCTURE amd64-match.S"
 */


#ifndef CURRENT_LINX_XCODE_MAC_X64_STRUCTURE
#define dsWSize		( 68)(%rdi)
#define dsWMask		( 76)(%rdi)
#define dsWindow	( 80)(%rdi)
#define dsPrev		( 96)(%rdi)
#define dsMatchLen	(144)(%rdi)
#define dsPrevMatch	(148)(%rdi)
#define dsStrStart	(156)(%rdi)
#define dsMatchStart	(160)(%rdi)
#define dsLookahead	(164)(%rdi)
#define dsPrevLen	(168)(%rdi)
#define dsMaxChainLen	(172)(%rdi)
#define dsGoodMatch	(188)(%rdi)
#define dsNiceMatch	(192)(%rdi)

#else 

#ifndef STRUCT_OFFSET
#	define STRUCT_OFFSET	(0)
#endif


#define dsWSize		( 56 + STRUCT_OFFSET)(%rdi)
#define dsWMask		( 64 + STRUCT_OFFSET)(%rdi)
#define dsWindow	( 72 + STRUCT_OFFSET)(%rdi)
#define dsPrev		( 88 + STRUCT_OFFSET)(%rdi)
#define dsMatchLen	(136 + STRUCT_OFFSET)(%rdi)
#define dsPrevMatch	(140 + STRUCT_OFFSET)(%rdi)
#define dsStrStart	(148 + STRUCT_OFFSET)(%rdi)
#define dsMatchStart	(152 + STRUCT_OFFSET)(%rdi)
#define dsLookahead	(156 + STRUCT_OFFSET)(%rdi)
#define dsPrevLen	(160 + STRUCT_OFFSET)(%rdi)
#define dsMaxChainLen	(164 + STRUCT_OFFSET)(%rdi)
#define dsGoodMatch	(180 + STRUCT_OFFSET)(%rdi)
#define dsNiceMatch	(184 + STRUCT_OFFSET)(%rdi)

#endif




.text

/* uInt longest_match(deflate_state *deflatestate, IPos curmatch) */

longest_match:
/*
 * Retrieve the function arguments. %curmatch will hold cur_match
 * throughout the entire function (passed via rsi on amd64).
 * rdi will hold the pointer to the deflate_state (first arg on amd64)
 */
		mov     %rsi, save_rsi
		mov     %rbx, save_rbx
		mov	%r12, save_r12
		mov     %r13, save_r13
		mov     %r14, save_r14
		mov     %r15, save_r15

/* uInt wmask = s->w_mask;						*/
/* unsigned chain_length = s->max_chain_length;				*/
/* if (s->prev_length >= s->good_match) {				*/
/*     chain_length >>= 2;						*/
/* }									*/

		movl	dsPrevLen, %eax
		movl	dsGoodMatch, %ebx
		cmpl	%ebx, %eax
		movl	dsWMask, %eax
		movl	dsMaxChainLen, %chainlenwmask
		jl	LastMatchGood
		shrl	$2, %chainlenwmask
LastMatchGood:

/* chainlen is decremented once beforehand so that the function can	*/
/* use the sign flag instead of the zero flag for the exit test.	*/
/* It is then shifted into the high word, to make room for the wmask	*/
/* value, which it will always accompany.				*/

		decl	%chainlenwmask
		shll	$16, %chainlenwmask
		orl	%eax, %chainlenwmask

/* if ((uInt)nice_match > s->lookahead) nice_match = s->lookahead;	*/

		movl	dsNiceMatch, %eax
		movl	dsLookahead, %ebx
		cmpl	%eax, %ebx
		jl	LookaheadLess
		movl	%eax, %ebx
LookaheadLess:	movl	%ebx, %nicematch

/* register Bytef *scan = s->window + s->strstart;			*/

		mov	dsWindow, %window
		movl	dsStrStart, %limitd
		lea	(%limit, %window), %scan

/* Determine how many bytes the scan ptr is off from being		*/
/* dword-aligned.							*/

		mov	%scan, %scanalign
		negl	%scanalignd
		andl	$3, %scanalignd

/* IPos limit = s->strstart > (IPos)MAX_DIST(s) ?			*/
/*     s->strstart - (IPos)MAX_DIST(s) : NIL;				*/

		movl	dsWSize, %eax
		subl	$MIN_LOOKAHEAD, %eax
		xorl	%ecx, %ecx
		subl	%eax, %limitd
		cmovng	%ecx, %limitd

/* int best_len = s->prev_length;					*/

		movl	dsPrevLen, %bestlend

/* Store the sum of s->window + best_len in %windowbestlen locally, and in memory.	*/

		lea	(%window, %bestlen), %windowbestlen
		mov	%windowbestlen, _windowbestlen

/* register ush scan_start = *(ushf*)scan;				*/
/* register ush scan_end   = *(ushf*)(scan+best_len-1);			*/
/* Posf *prev = s->prev;						*/

		movzwl	(%scan), %scanstart
		movzwl	-1(%scan, %bestlen), %scanend
		mov	dsPrev, %prev

/* Jump into the main loop.						*/

		movl	%chainlenwmask, _chainlenwmask
		jmp	LoopEntry

.balign 16

/* do {
 *     match = s->window + cur_match;
 *     if (*(ushf*)(match+best_len-1) != scan_end ||
 *         *(ushf*)match != scan_start) continue;
 *     [...]
 * } while ((cur_match = prev[cur_match & wmask]) > limit
 *          && --chain_length != 0);
 *
 * Here is the inner loop of the function. The function will spend the
 * majority of its time in this loop, and majority of that time will
 * be spent in the first ten instructions.
 */
LookupLoop:
		andl	%chainlenwmask, %curmatchd
		movzwl	(%prev, %curmatch, 2), %curmatchd
		cmpl	%limitd, %curmatchd
		jbe	LeaveNow
		subl	$0x00010000, %chainlenwmask
		js	LeaveNow
LoopEntry:	cmpw	-1(%windowbestlen, %curmatch), %scanendw
		jne	LookupLoop
		cmpw	%scanstartw, (%window, %curmatch)
		jne	LookupLoop

/* Store the current value of chainlen.					*/
		movl	%chainlenwmask, _chainlenwmask

/* %scan is the string under scrutiny, and %prev to the string we	*/
/* are hoping to match it up with. In actuality, %esi and %edi are	*/
/* both pointed (MAX_MATCH_8 - scanalign) bytes ahead, and %edx is	*/
/* initialized to -(MAX_MATCH_8 - scanalign).				*/

		mov	$(-MAX_MATCH_8), %rdx
		lea	(%curmatch, %window), %windowbestlen
		lea	MAX_MATCH_8(%windowbestlen, %scanalign), %windowbestlen
		lea	MAX_MATCH_8(%scan, %scanalign), %prev

/* the prefetching below makes very little difference... */
		prefetcht1	(%windowbestlen, %rdx)
		prefetcht1	(%prev, %rdx)

/*
 * Test the strings for equality, 8 bytes at a time. At the end,
 * adjust %rdx so that it is offset to the exact byte that mismatched.
 *
 * It should be confessed that this loop usually does not represent
 * much of the total running time. Replacing it with a more
 * straightforward "rep cmpsb" would not drastically degrade
 * performance -- unrolling it, for example, makes no difference.
 */

#undef USE_SSE	/* works, but is 6-7% slower, than non-SSE... */

LoopCmps:
#ifdef USE_SSE
		/* Preload the SSE registers */
		movdqu	  (%windowbestlen, %rdx), %xmm1
		movdqu	  (%prev, %rdx), %xmm2
		pcmpeqb	%xmm2, %xmm1
		movdqu	16(%windowbestlen, %rdx), %xmm3
		movdqu	16(%prev, %rdx), %xmm4
		pcmpeqb	%xmm4, %xmm3
		movdqu	32(%windowbestlen, %rdx), %xmm5
		movdqu	32(%prev, %rdx), %xmm6
		pcmpeqb	%xmm6, %xmm5
		movdqu	48(%windowbestlen, %rdx), %xmm7
		movdqu	48(%prev, %rdx), %xmm8
		pcmpeqb	%xmm8, %xmm7

		/* Check the comparisions' results */
		pmovmskb %xmm1, %rax
		notw	%ax
		bsfw	%ax, %ax
		jnz	LeaveLoopCmps
		
		/* this is the only iteration of the loop with a possibility of having
		   incremented rdx by 0x108 (each loop iteration add 16*4 = 0x40 
		   and (0x40*4)+8=0x108 */
		add	$8, %rdx
		jz LenMaximum
		add	$8, %rdx

		
		pmovmskb %xmm3, %rax
		notw	%ax
		bsfw	%ax, %ax
		jnz	LeaveLoopCmps
		
		
		add	$16, %rdx


		pmovmskb %xmm5, %rax
		notw	%ax
		bsfw	%ax, %ax
		jnz	LeaveLoopCmps
		
		add	$16, %rdx


		pmovmskb %xmm7, %rax
		notw	%ax
		bsfw	%ax, %ax
		jnz	LeaveLoopCmps
		
		add	$16, %rdx
		
		jmp	LoopCmps
LeaveLoopCmps:	add	%rax, %rdx
#else
		mov	(%windowbestlen, %rdx), %rax
		xor	(%prev, %rdx), %rax
		jnz	LeaveLoopCmps
		
		mov	8(%windowbestlen, %rdx), %rax
		xor	8(%prev, %rdx), %rax
		jnz	LeaveLoopCmps8

		mov	16(%windowbestlen, %rdx), %rax
		xor	16(%prev, %rdx), %rax
		jnz	LeaveLoopCmps16
				
		add	$24, %rdx
		jnz	LoopCmps
		jmp	LenMaximum
#	if 0
/*
 * This three-liner is tantalizingly simple, but bsf is a slow instruction,
 * and the complicated alternative down below is quite a bit faster. Sad...
 */

LeaveLoopCmps:	bsf	%rax, %rax /* find the first non-zero bit */
		shrl	$3, %eax /* divide by 8 to get the byte */
		add	%rax, %rdx
#	else
LeaveLoopCmps16:
		add	$8, %rdx
LeaveLoopCmps8:
		add	$8, %rdx
LeaveLoopCmps:	testl   $0xFFFFFFFF, %eax /* Check the first 4 bytes */
		jnz     Check16
		add     $4, %rdx
		shr     $32, %rax
Check16:        testw   $0xFFFF, %ax
		jnz     LenLower
		add	$2, %rdx
		shrl	$16, %eax
LenLower:	subb	$1, %al
		adc	$0, %rdx
#	endif
#endif

/* Calculate the length of the match. If it is longer than MAX_MATCH,	*/
/* then automatically accept it as the best possible match and leave.	*/

		lea	(%prev, %rdx), %rax
		sub	%scan, %rax
		cmpl	$MAX_MATCH, %eax
		jge	LenMaximum

/* If the length of the match is not longer than the best match we	*/
/* have so far, then forget it and return to the lookup loop.		*/

		cmpl	%bestlend, %eax
		jg	LongerMatch
		mov	_windowbestlen, %windowbestlen
		mov	dsPrev, %prev
		movl	_chainlenwmask, %edx
		jmp	LookupLoop

/*         s->match_start = cur_match;					*/
/*         best_len = len;						*/
/*         if (len >= nice_match) break;				*/
/*         scan_end = *(ushf*)(scan+best_len-1);			*/

LongerMatch:
		movl	%eax, %bestlend
		movl	%curmatchd, dsMatchStart
		cmpl	%nicematch, %eax
		jge	LeaveNow

		lea	(%window, %bestlen), %windowbestlen
		mov	%windowbestlen, _windowbestlen

		movzwl	-1(%scan, %rax), %scanend
		mov	dsPrev, %prev
		movl	_chainlenwmask, %chainlenwmask
		jmp	LookupLoop

/* Accept the current string, with the maximum possible length.		*/

LenMaximum:
		movl	$MAX_MATCH, %bestlend
		movl	%curmatchd, dsMatchStart

/* if ((uInt)best_len <= s->lookahead) return (uInt)best_len;		*/
/* return s->lookahead;							*/

LeaveNow:
		movl	dsLookahead, %eax
		cmpl	%eax, %bestlend
		cmovngl	%bestlend, %eax
LookaheadRet:

/* Restore the registers and return from whence we came.			*/

	mov	save_rsi, %rsi
	mov	save_rbx, %rbx
	mov	save_r12, %r12
	mov	save_r13, %r13
	mov	save_r14, %r14
	mov	save_r15, %r15

	ret

match_init:	ret
