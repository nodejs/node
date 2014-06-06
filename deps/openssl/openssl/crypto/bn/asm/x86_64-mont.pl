#!/usr/bin/env perl

# ====================================================================
# Written by Andy Polyakov <appro@fy.chalmers.se> for the OpenSSL
# project. The module is, however, dual licensed under OpenSSL and
# CRYPTOGAMS licenses depending on where you obtain it. For further
# details see http://www.openssl.org/~appro/cryptogams/.
# ====================================================================

# October 2005.
#
# Montgomery multiplication routine for x86_64. While it gives modest
# 9% improvement of rsa4096 sign on Opteron, rsa512 sign runs more
# than twice, >2x, as fast. Most common rsa1024 sign is improved by
# respectful 50%. It remains to be seen if loop unrolling and
# dedicated squaring routine can provide further improvement...

$flavour = shift;
$output  = shift;
if ($flavour =~ /\./) { $output = $flavour; undef $flavour; }

$win64=0; $win64=1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../perlasm/x86_64-xlate.pl" and -f $xlate) or
die "can't locate x86_64-xlate.pl";

open OUT,"| \"$^X\" $xlate $flavour $output";
*STDOUT=*OUT;

# int bn_mul_mont(
$rp="%rdi";	# BN_ULONG *rp,
$ap="%rsi";	# const BN_ULONG *ap,
$bp="%rdx";	# const BN_ULONG *bp,
$np="%rcx";	# const BN_ULONG *np,
$n0="%r8";	# const BN_ULONG *n0,
$num="%r9";	# int num);
$lo0="%r10";
$hi0="%r11";
$bp="%r12";	# reassign $bp
$hi1="%r13";
$i="%r14";
$j="%r15";
$m0="%rbx";
$m1="%rbp";

$code=<<___;
.text

.globl	bn_mul_mont
.type	bn_mul_mont,\@function,6
.align	16
bn_mul_mont:
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15

	mov	${num}d,${num}d
	lea	2($num),%r10
	mov	%rsp,%r11
	neg	%r10
	lea	(%rsp,%r10,8),%rsp	# tp=alloca(8*(num+2))
	and	\$-1024,%rsp		# minimize TLB usage

	mov	%r11,8(%rsp,$num,8)	# tp[num+1]=%rsp
.Lprologue:
	mov	%rdx,$bp		# $bp reassigned, remember?

	mov	($n0),$n0		# pull n0[0] value

	xor	$i,$i			# i=0
	xor	$j,$j			# j=0

	mov	($bp),$m0		# m0=bp[0]
	mov	($ap),%rax
	mulq	$m0			# ap[0]*bp[0]
	mov	%rax,$lo0
	mov	%rdx,$hi0

	imulq	$n0,%rax		# "tp[0]"*n0
	mov	%rax,$m1

	mulq	($np)			# np[0]*m1
	add	$lo0,%rax		# discarded
	adc	\$0,%rdx
	mov	%rdx,$hi1

	lea	1($j),$j		# j++
.L1st:
	mov	($ap,$j,8),%rax
	mulq	$m0			# ap[j]*bp[0]
	add	$hi0,%rax
	adc	\$0,%rdx
	mov	%rax,$lo0
	mov	($np,$j,8),%rax
	mov	%rdx,$hi0

	mulq	$m1			# np[j]*m1
	add	$hi1,%rax
	lea	1($j),$j		# j++
	adc	\$0,%rdx
	add	$lo0,%rax		# np[j]*m1+ap[j]*bp[0]
	adc	\$0,%rdx
	mov	%rax,-16(%rsp,$j,8)	# tp[j-1]
	cmp	$num,$j
	mov	%rdx,$hi1
	jl	.L1st

	xor	%rdx,%rdx
	add	$hi0,$hi1
	adc	\$0,%rdx
	mov	$hi1,-8(%rsp,$num,8)
	mov	%rdx,(%rsp,$num,8)	# store upmost overflow bit

	lea	1($i),$i		# i++
.align	4
.Louter:
	xor	$j,$j			# j=0

	mov	($bp,$i,8),$m0		# m0=bp[i]
	mov	($ap),%rax		# ap[0]
	mulq	$m0			# ap[0]*bp[i]
	add	(%rsp),%rax		# ap[0]*bp[i]+tp[0]
	adc	\$0,%rdx
	mov	%rax,$lo0
	mov	%rdx,$hi0

	imulq	$n0,%rax		# tp[0]*n0
	mov	%rax,$m1

	mulq	($np,$j,8)		# np[0]*m1
	add	$lo0,%rax		# discarded
	mov	8(%rsp),$lo0		# tp[1]
	adc	\$0,%rdx
	mov	%rdx,$hi1

	lea	1($j),$j		# j++
.align	4
.Linner:
	mov	($ap,$j,8),%rax
	mulq	$m0			# ap[j]*bp[i]
	add	$hi0,%rax
	adc	\$0,%rdx
	add	%rax,$lo0		# ap[j]*bp[i]+tp[j]
	mov	($np,$j,8),%rax
	adc	\$0,%rdx
	mov	%rdx,$hi0

	mulq	$m1			# np[j]*m1
	add	$hi1,%rax
	lea	1($j),$j		# j++
	adc	\$0,%rdx
	add	$lo0,%rax		# np[j]*m1+ap[j]*bp[i]+tp[j]
	adc	\$0,%rdx
	mov	(%rsp,$j,8),$lo0
	cmp	$num,$j
	mov	%rax,-16(%rsp,$j,8)	# tp[j-1]
	mov	%rdx,$hi1
	jl	.Linner

	xor	%rdx,%rdx
	add	$hi0,$hi1
	adc	\$0,%rdx
	add	$lo0,$hi1		# pull upmost overflow bit
	adc	\$0,%rdx
	mov	$hi1,-8(%rsp,$num,8)
	mov	%rdx,(%rsp,$num,8)	# store upmost overflow bit

	lea	1($i),$i		# i++
	cmp	$num,$i
	jl	.Louter

	lea	(%rsp),$ap		# borrow ap for tp
	lea	-1($num),$j		# j=num-1

	mov	($ap),%rax		# tp[0]
	xor	$i,$i			# i=0 and clear CF!
	jmp	.Lsub
.align	16
.Lsub:	sbb	($np,$i,8),%rax
	mov	%rax,($rp,$i,8)		# rp[i]=tp[i]-np[i]
	dec	$j			# doesn't affect CF!
	mov	8($ap,$i,8),%rax	# tp[i+1]
	lea	1($i),$i		# i++
	jge	.Lsub

	sbb	\$0,%rax		# handle upmost overflow bit
	and	%rax,$ap
	not	%rax
	mov	$rp,$np
	and	%rax,$np
	lea	-1($num),$j
	or	$np,$ap			# ap=borrow?tp:rp
.align	16
.Lcopy:					# copy or in-place refresh
	mov	($ap,$j,8),%rax
	mov	%rax,($rp,$j,8)		# rp[i]=tp[i]
	mov	$i,(%rsp,$j,8)		# zap temporary vector
	dec	$j
	jge	.Lcopy

	mov	8(%rsp,$num,8),%rsi	# restore %rsp
	mov	\$1,%rax
	mov	(%rsi),%r15
	mov	8(%rsi),%r14
	mov	16(%rsi),%r13
	mov	24(%rsi),%r12
	mov	32(%rsi),%rbp
	mov	40(%rsi),%rbx
	lea	48(%rsi),%rsp
.Lepilogue:
	ret
.size	bn_mul_mont,.-bn_mul_mont
.asciz	"Montgomery Multiplication for x86_64, CRYPTOGAMS by <appro\@openssl.org>"
.align	16
___

# EXCEPTION_DISPOSITION handler (EXCEPTION_RECORD *rec,ULONG64 frame,
#		CONTEXT *context,DISPATCHER_CONTEXT *disp)
if ($win64) {
$rec="%rcx";
$frame="%rdx";
$context="%r8";
$disp="%r9";

$code.=<<___;
.extern	__imp_RtlVirtualUnwind
.type	se_handler,\@abi-omnipotent
.align	16
se_handler:
	push	%rsi
	push	%rdi
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	pushfq
	sub	\$64,%rsp

	mov	120($context),%rax	# pull context->Rax
	mov	248($context),%rbx	# pull context->Rip

	lea	.Lprologue(%rip),%r10
	cmp	%r10,%rbx		# context->Rip<.Lprologue
	jb	.Lin_prologue

	mov	152($context),%rax	# pull context->Rsp

	lea	.Lepilogue(%rip),%r10
	cmp	%r10,%rbx		# context->Rip>=.Lepilogue
	jae	.Lin_prologue

	mov	192($context),%r10	# pull $num
	mov	8(%rax,%r10,8),%rax	# pull saved stack pointer
	lea	48(%rax),%rax

	mov	-8(%rax),%rbx
	mov	-16(%rax),%rbp
	mov	-24(%rax),%r12
	mov	-32(%rax),%r13
	mov	-40(%rax),%r14
	mov	-48(%rax),%r15
	mov	%rbx,144($context)	# restore context->Rbx
	mov	%rbp,160($context)	# restore context->Rbp
	mov	%r12,216($context)	# restore context->R12
	mov	%r13,224($context)	# restore context->R13
	mov	%r14,232($context)	# restore context->R14
	mov	%r15,240($context)	# restore context->R15

.Lin_prologue:
	mov	8(%rax),%rdi
	mov	16(%rax),%rsi
	mov	%rax,152($context)	# restore context->Rsp
	mov	%rsi,168($context)	# restore context->Rsi
	mov	%rdi,176($context)	# restore context->Rdi

	mov	40($disp),%rdi		# disp->ContextRecord
	mov	$context,%rsi		# context
	mov	\$154,%ecx		# sizeof(CONTEXT)
	.long	0xa548f3fc		# cld; rep movsq

	mov	$disp,%rsi
	xor	%rcx,%rcx		# arg1, UNW_FLAG_NHANDLER
	mov	8(%rsi),%rdx		# arg2, disp->ImageBase
	mov	0(%rsi),%r8		# arg3, disp->ControlPc
	mov	16(%rsi),%r9		# arg4, disp->FunctionEntry
	mov	40(%rsi),%r10		# disp->ContextRecord
	lea	56(%rsi),%r11		# &disp->HandlerData
	lea	24(%rsi),%r12		# &disp->EstablisherFrame
	mov	%r10,32(%rsp)		# arg5
	mov	%r11,40(%rsp)		# arg6
	mov	%r12,48(%rsp)		# arg7
	mov	%rcx,56(%rsp)		# arg8, (NULL)
	call	*__imp_RtlVirtualUnwind(%rip)

	mov	\$1,%eax		# ExceptionContinueSearch
	add	\$64,%rsp
	popfq
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	pop	%rdi
	pop	%rsi
	ret
.size	se_handler,.-se_handler

.section	.pdata
.align	4
	.rva	.LSEH_begin_bn_mul_mont
	.rva	.LSEH_end_bn_mul_mont
	.rva	.LSEH_info_bn_mul_mont

.section	.xdata
.align	8
.LSEH_info_bn_mul_mont:
	.byte	9,0,0,0
	.rva	se_handler
___
}

print $code;
close STDOUT;
