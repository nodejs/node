#!/usr/bin/env perl
#
# ====================================================================
# Written by Andy Polyakov <appro@fy.chalmers.se> for the OpenSSL
# project. The module is, however, dual licensed under OpenSSL and
# CRYPTOGAMS licenses depending on where you obtain it. For further
# details see http://www.openssl.org/~appro/cryptogams/.
# ====================================================================
#
# 2.22x RC4 tune-up:-) It should be noted though that my hand [as in
# "hand-coded assembler"] doesn't stand for the whole improvement
# coefficient. It turned out that eliminating RC4_CHAR from config
# line results in ~40% improvement (yes, even for C implementation).
# Presumably it has everything to do with AMD cache architecture and
# RAW or whatever penalties. Once again! The module *requires* config
# line *without* RC4_CHAR! As for coding "secret," I bet on partial
# register arithmetics. For example instead of 'inc %r8; and $255,%r8'
# I simply 'inc %r8b'. Even though optimization manual discourages
# to operate on partial registers, it turned out to be the best bet.
# At least for AMD... How IA32E would perform remains to be seen...

# As was shown by Marc Bevand reordering of couple of load operations
# results in even higher performance gain of 3.3x:-) At least on
# Opteron... For reference, 1x in this case is RC4_CHAR C-code
# compiled with gcc 3.3.2, which performs at ~54MBps per 1GHz clock.
# Latter means that if you want to *estimate* what to expect from
# *your* Opteron, then multiply 54 by 3.3 and clock frequency in GHz.

# Intel P4 EM64T core was found to run the AMD64 code really slow...
# The only way to achieve comparable performance on P4 was to keep
# RC4_CHAR. Kind of ironic, huh? As it's apparently impossible to
# compose blended code, which would perform even within 30% marginal
# on either AMD and Intel platforms, I implement both cases. See
# rc4_skey.c for further details...

# P4 EM64T core appears to be "allergic" to 64-bit inc/dec. Replacing 
# those with add/sub results in 50% performance improvement of folded
# loop...

# As was shown by Zou Nanhai loop unrolling can improve Intel EM64T
# performance by >30% [unlike P4 32-bit case that is]. But this is
# provided that loads are reordered even more aggressively! Both code
# pathes, AMD64 and EM64T, reorder loads in essentially same manner
# as my IA-64 implementation. On Opteron this resulted in modest 5%
# improvement [I had to test it], while final Intel P4 performance
# achieves respectful 432MBps on 2.8GHz processor now. For reference.
# If executed on Xeon, current RC4_CHAR code-path is 2.7x faster than
# RC4_INT code-path. While if executed on Opteron, it's only 25%
# slower than the RC4_INT one [meaning that if CPU µ-arch detection
# is not implemented, then this final RC4_CHAR code-path should be
# preferred, as it provides better *all-round* performance].

# Intel Core2 was observed to perform poorly on both code paths:-( It
# apparently suffers from some kind of partial register stall, which
# occurs in 64-bit mode only [as virtually identical 32-bit loop was
# observed to outperform 64-bit one by almost 50%]. Adding two movzb to
# cloop1 boosts its performance by 80%! This loop appears to be optimal
# fit for Core2 and therefore the code was modified to skip cloop8 on
# this CPU.

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

$dat="%rdi";	    # arg1
$len="%rsi";	    # arg2
$inp="%rdx";	    # arg3
$out="%rcx";	    # arg4

@XX=("%r8","%r10");
@TX=("%r9","%r11");
$YY="%r12";
$TY="%r13";

$code=<<___;
.text

.globl	RC4
.type	RC4,\@function,4
.align	16
RC4:	or	$len,$len
	jne	.Lentry
	ret
.Lentry:
	push	%rbx
	push	%r12
	push	%r13
.Lprologue:

	add	\$8,$dat
	movl	-8($dat),$XX[0]#d
	movl	-4($dat),$YY#d
	cmpl	\$-1,256($dat)
	je	.LRC4_CHAR
	inc	$XX[0]#b
	movl	($dat,$XX[0],4),$TX[0]#d
	test	\$-8,$len
	jz	.Lloop1
	jmp	.Lloop8
.align	16
.Lloop8:
___
for ($i=0;$i<8;$i++) {
$code.=<<___;
	add	$TX[0]#b,$YY#b
	mov	$XX[0],$XX[1]
	movl	($dat,$YY,4),$TY#d
	ror	\$8,%rax			# ror is redundant when $i=0
	inc	$XX[1]#b
	movl	($dat,$XX[1],4),$TX[1]#d
	cmp	$XX[1],$YY
	movl	$TX[0]#d,($dat,$YY,4)
	cmove	$TX[0],$TX[1]
	movl	$TY#d,($dat,$XX[0],4)
	add	$TX[0]#b,$TY#b
	movb	($dat,$TY,4),%al
___
push(@TX,shift(@TX)); push(@XX,shift(@XX));	# "rotate" registers
}
$code.=<<___;
	ror	\$8,%rax
	sub	\$8,$len

	xor	($inp),%rax
	add	\$8,$inp
	mov	%rax,($out)
	add	\$8,$out

	test	\$-8,$len
	jnz	.Lloop8
	cmp	\$0,$len
	jne	.Lloop1
	jmp	.Lexit

.align	16
.Lloop1:
	add	$TX[0]#b,$YY#b
	movl	($dat,$YY,4),$TY#d
	movl	$TX[0]#d,($dat,$YY,4)
	movl	$TY#d,($dat,$XX[0],4)
	add	$TY#b,$TX[0]#b
	inc	$XX[0]#b
	movl	($dat,$TX[0],4),$TY#d
	movl	($dat,$XX[0],4),$TX[0]#d
	xorb	($inp),$TY#b
	inc	$inp
	movb	$TY#b,($out)
	inc	$out
	dec	$len
	jnz	.Lloop1
	jmp	.Lexit

.align	16
.LRC4_CHAR:
	add	\$1,$XX[0]#b
	movzb	($dat,$XX[0]),$TX[0]#d
	test	\$-8,$len
	jz	.Lcloop1
	cmpl	\$0,260($dat)
	jnz	.Lcloop1
	jmp	.Lcloop8
.align	16
.Lcloop8:
	mov	($inp),%eax
	mov	4($inp),%ebx
___
# unroll 2x4-wise, because 64-bit rotates kill Intel P4...
for ($i=0;$i<4;$i++) {
$code.=<<___;
	add	$TX[0]#b,$YY#b
	lea	1($XX[0]),$XX[1]
	movzb	($dat,$YY),$TY#d
	movzb	$XX[1]#b,$XX[1]#d
	movzb	($dat,$XX[1]),$TX[1]#d
	movb	$TX[0]#b,($dat,$YY)
	cmp	$XX[1],$YY
	movb	$TY#b,($dat,$XX[0])
	jne	.Lcmov$i			# Intel cmov is sloooow...
	mov	$TX[0],$TX[1]
.Lcmov$i:
	add	$TX[0]#b,$TY#b
	xor	($dat,$TY),%al
	ror	\$8,%eax
___
push(@TX,shift(@TX)); push(@XX,shift(@XX));	# "rotate" registers
}
for ($i=4;$i<8;$i++) {
$code.=<<___;
	add	$TX[0]#b,$YY#b
	lea	1($XX[0]),$XX[1]
	movzb	($dat,$YY),$TY#d
	movzb	$XX[1]#b,$XX[1]#d
	movzb	($dat,$XX[1]),$TX[1]#d
	movb	$TX[0]#b,($dat,$YY)
	cmp	$XX[1],$YY
	movb	$TY#b,($dat,$XX[0])
	jne	.Lcmov$i			# Intel cmov is sloooow...
	mov	$TX[0],$TX[1]
.Lcmov$i:
	add	$TX[0]#b,$TY#b
	xor	($dat,$TY),%bl
	ror	\$8,%ebx
___
push(@TX,shift(@TX)); push(@XX,shift(@XX));	# "rotate" registers
}
$code.=<<___;
	lea	-8($len),$len
	mov	%eax,($out)
	lea	8($inp),$inp
	mov	%ebx,4($out)
	lea	8($out),$out

	test	\$-8,$len
	jnz	.Lcloop8
	cmp	\$0,$len
	jne	.Lcloop1
	jmp	.Lexit
___
$code.=<<___;
.align	16
.Lcloop1:
	add	$TX[0]#b,$YY#b
	movzb	($dat,$YY),$TY#d
	movb	$TX[0]#b,($dat,$YY)
	movb	$TY#b,($dat,$XX[0])
	add	$TX[0]#b,$TY#b
	add	\$1,$XX[0]#b
	movzb	$TY#b,$TY#d
	movzb	$XX[0]#b,$XX[0]#d
	movzb	($dat,$TY),$TY#d
	movzb	($dat,$XX[0]),$TX[0]#d
	xorb	($inp),$TY#b
	lea	1($inp),$inp
	movb	$TY#b,($out)
	lea	1($out),$out
	sub	\$1,$len
	jnz	.Lcloop1
	jmp	.Lexit

.align	16
.Lexit:
	sub	\$1,$XX[0]#b
	movl	$XX[0]#d,-8($dat)
	movl	$YY#d,-4($dat)

	mov	(%rsp),%r13
	mov	8(%rsp),%r12
	mov	16(%rsp),%rbx
	add	\$24,%rsp
.Lepilogue:
	ret
.size	RC4,.-RC4
___

$idx="%r8";
$ido="%r9";

$code.=<<___;
.extern	OPENSSL_ia32cap_P
.globl	RC4_set_key
.type	RC4_set_key,\@function,3
.align	16
RC4_set_key:
	lea	8($dat),$dat
	lea	($inp,$len),$inp
	neg	$len
	mov	$len,%rcx
	xor	%eax,%eax
	xor	$ido,$ido
	xor	%r10,%r10
	xor	%r11,%r11

	mov	OPENSSL_ia32cap_P(%rip),$idx#d
	bt	\$20,$idx#d
	jnc	.Lw1stloop
	bt	\$30,$idx#d
	setc	$ido#b
	mov	$ido#d,260($dat)
	jmp	.Lc1stloop

.align	16
.Lw1stloop:
	mov	%eax,($dat,%rax,4)
	add	\$1,%al
	jnc	.Lw1stloop

	xor	$ido,$ido
	xor	$idx,$idx
.align	16
.Lw2ndloop:
	mov	($dat,$ido,4),%r10d
	add	($inp,$len,1),$idx#b
	add	%r10b,$idx#b
	add	\$1,$len
	mov	($dat,$idx,4),%r11d
	cmovz	%rcx,$len
	mov	%r10d,($dat,$idx,4)
	mov	%r11d,($dat,$ido,4)
	add	\$1,$ido#b
	jnc	.Lw2ndloop
	jmp	.Lexit_key

.align	16
.Lc1stloop:
	mov	%al,($dat,%rax)
	add	\$1,%al
	jnc	.Lc1stloop

	xor	$ido,$ido
	xor	$idx,$idx
.align	16
.Lc2ndloop:
	mov	($dat,$ido),%r10b
	add	($inp,$len),$idx#b
	add	%r10b,$idx#b
	add	\$1,$len
	mov	($dat,$idx),%r11b
	jnz	.Lcnowrap
	mov	%rcx,$len
.Lcnowrap:
	mov	%r10b,($dat,$idx)
	mov	%r11b,($dat,$ido)
	add	\$1,$ido#b
	jnc	.Lc2ndloop
	movl	\$-1,256($dat)

.align	16
.Lexit_key:
	xor	%eax,%eax
	mov	%eax,-8($dat)
	mov	%eax,-4($dat)
	ret
.size	RC4_set_key,.-RC4_set_key

.globl	RC4_options
.type	RC4_options,\@abi-omnipotent
.align	16
RC4_options:
	lea	.Lopts(%rip),%rax
	mov	OPENSSL_ia32cap_P(%rip),%edx
	bt	\$20,%edx
	jnc	.Ldone
	add	\$12,%rax
	bt	\$30,%edx
	jnc	.Ldone
	add	\$13,%rax
.Ldone:
	ret
.align	64
.Lopts:
.asciz	"rc4(8x,int)"
.asciz	"rc4(8x,char)"
.asciz	"rc4(1x,char)"
.asciz	"RC4 for x86_64, CRYPTOGAMS by <appro\@openssl.org>"
.align	64
.size	RC4_options,.-RC4_options
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
.type	stream_se_handler,\@abi-omnipotent
.align	16
stream_se_handler:
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
	cmp	%r10,%rbx		# context->Rip<prologue label
	jb	.Lin_prologue

	mov	152($context),%rax	# pull context->Rsp

	lea	.Lepilogue(%rip),%r10
	cmp	%r10,%rbx		# context->Rip>=epilogue label
	jae	.Lin_prologue

	lea	24(%rax),%rax

	mov	-8(%rax),%rbx
	mov	-16(%rax),%r12
	mov	-24(%rax),%r13
	mov	%rbx,144($context)	# restore context->Rbx
	mov	%r12,216($context)	# restore context->R12
	mov	%r13,224($context)	# restore context->R13

.Lin_prologue:
	mov	8(%rax),%rdi
	mov	16(%rax),%rsi
	mov	%rax,152($context)	# restore context->Rsp
	mov	%rsi,168($context)	# restore context->Rsi
	mov	%rdi,176($context)	# restore context->Rdi

	jmp	.Lcommon_seh_exit
.size	stream_se_handler,.-stream_se_handler

.type	key_se_handler,\@abi-omnipotent
.align	16
key_se_handler:
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

	mov	152($context),%rax	# pull context->Rsp
	mov	8(%rax),%rdi
	mov	16(%rax),%rsi
	mov	%rsi,168($context)	# restore context->Rsi
	mov	%rdi,176($context)	# restore context->Rdi

.Lcommon_seh_exit:

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
.size	key_se_handler,.-key_se_handler

.section	.pdata
.align	4
	.rva	.LSEH_begin_RC4
	.rva	.LSEH_end_RC4
	.rva	.LSEH_info_RC4

	.rva	.LSEH_begin_RC4_set_key
	.rva	.LSEH_end_RC4_set_key
	.rva	.LSEH_info_RC4_set_key

.section	.xdata
.align	8
.LSEH_info_RC4:
	.byte	9,0,0,0
	.rva	stream_se_handler
.LSEH_info_RC4_set_key:
	.byte	9,0,0,0
	.rva	key_se_handler
___
}

$code =~ s/#([bwd])/$1/gm;

print $code;

close STDOUT;
