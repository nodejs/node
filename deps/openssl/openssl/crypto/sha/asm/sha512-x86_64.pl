#!/usr/bin/env perl
#
# ====================================================================
# Written by Andy Polyakov <appro@fy.chalmers.se> for the OpenSSL
# project. Rights for redistribution and usage in source and binary
# forms are granted according to the OpenSSL license.
# ====================================================================
#
# sha256/512_block procedure for x86_64.
#
# 40% improvement over compiler-generated code on Opteron. On EM64T
# sha256 was observed to run >80% faster and sha512 - >40%. No magical
# tricks, just straight implementation... I really wonder why gcc
# [being armed with inline assembler] fails to generate as fast code.
# The only thing which is cool about this module is that it's very
# same instruction sequence used for both SHA-256 and SHA-512. In
# former case the instructions operate on 32-bit operands, while in
# latter - on 64-bit ones. All I had to do is to get one flavor right,
# the other one passed the test right away:-)
#
# sha256_block runs in ~1005 cycles on Opteron, which gives you
# asymptotic performance of 64*1000/1005=63.7MBps times CPU clock
# frequency in GHz. sha512_block runs in ~1275 cycles, which results
# in 128*1000/1275=100MBps per GHz. Is there room for improvement?
# Well, if you compare it to IA-64 implementation, which maintains
# X[16] in register bank[!], tends to 4 instructions per CPU clock
# cycle and runs in 1003 cycles, 1275 is very good result for 3-way
# issue Opteron pipeline and X[16] maintained in memory. So that *if*
# there is a way to improve it, *then* the only way would be to try to
# offload X[16] updates to SSE unit, but that would require "deeper"
# loop unroll, which in turn would naturally cause size blow-up, not
# to mention increased complexity! And once again, only *if* it's
# actually possible to noticeably improve overall ILP, instruction
# level parallelism, on a given CPU implementation in this case.
#
# Special note on Intel EM64T. While Opteron CPU exhibits perfect
# perfromance ratio of 1.5 between 64- and 32-bit flavors [see above],
# [currently available] EM64T CPUs apparently are far from it. On the
# contrary, 64-bit version, sha512_block, is ~30% *slower* than 32-bit
# sha256_block:-( This is presumably because 64-bit shifts/rotates
# apparently are not atomic instructions, but implemented in microcode.

$output=shift;

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../perlasm/x86_64-xlate.pl" and -f $xlate) or
die "can't locate x86_64-xlate.pl";

open STDOUT,"| $^X $xlate $output";

if ($output =~ /512/) {
	$func="sha512_block_data_order";
	$TABLE="K512";
	$SZ=8;
	@ROT=($A,$B,$C,$D,$E,$F,$G,$H)=("%rax","%rbx","%rcx","%rdx",
					"%r8", "%r9", "%r10","%r11");
	($T1,$a0,$a1,$a2)=("%r12","%r13","%r14","%r15");
	@Sigma0=(28,34,39);
	@Sigma1=(14,18,41);
	@sigma0=(1,  8, 7);
	@sigma1=(19,61, 6);
	$rounds=80;
} else {
	$func="sha256_block_data_order";
	$TABLE="K256";
	$SZ=4;
	@ROT=($A,$B,$C,$D,$E,$F,$G,$H)=("%eax","%ebx","%ecx","%edx",
					"%r8d","%r9d","%r10d","%r11d");
	($T1,$a0,$a1,$a2)=("%r12d","%r13d","%r14d","%r15d");
	@Sigma0=( 2,13,22);
	@Sigma1=( 6,11,25);
	@sigma0=( 7,18, 3);
	@sigma1=(17,19,10);
	$rounds=64;
}

$ctx="%rdi";	# 1st arg
$round="%rdi";	# zaps $ctx
$inp="%rsi";	# 2nd arg
$Tbl="%rbp";

$_ctx="16*$SZ+0*8(%rsp)";
$_inp="16*$SZ+1*8(%rsp)";
$_end="16*$SZ+2*8(%rsp)";
$_rsp="16*$SZ+3*8(%rsp)";
$framesz="16*$SZ+4*8";


sub ROUND_00_15()
{ my ($i,$a,$b,$c,$d,$e,$f,$g,$h) = @_;

$code.=<<___;
	mov	$e,$a0
	mov	$e,$a1
	mov	$f,$a2

	ror	\$$Sigma1[0],$a0
	ror	\$$Sigma1[1],$a1
	xor	$g,$a2			# f^g

	xor	$a1,$a0
	ror	\$`$Sigma1[2]-$Sigma1[1]`,$a1
	and	$e,$a2			# (f^g)&e
	mov	$T1,`$SZ*($i&0xf)`(%rsp)

	xor	$a1,$a0			# Sigma1(e)
	xor	$g,$a2			# Ch(e,f,g)=((f^g)&e)^g
	add	$h,$T1			# T1+=h

	mov	$a,$h
	add	$a0,$T1			# T1+=Sigma1(e)

	add	$a2,$T1			# T1+=Ch(e,f,g)
	mov	$a,$a0
	mov	$a,$a1

	ror	\$$Sigma0[0],$h
	ror	\$$Sigma0[1],$a0
	mov	$a,$a2
	add	($Tbl,$round,$SZ),$T1	# T1+=K[round]

	xor	$a0,$h
	ror	\$`$Sigma0[2]-$Sigma0[1]`,$a0
	or	$c,$a1			# a|c

	xor	$a0,$h			# h=Sigma0(a)
	and	$c,$a2			# a&c
	add	$T1,$d			# d+=T1

	and	$b,$a1			# (a|c)&b
	add	$T1,$h			# h+=T1

	or	$a2,$a1			# Maj(a,b,c)=((a|c)&b)|(a&c)
	lea	1($round),$round	# round++

	add	$a1,$h			# h+=Maj(a,b,c)
___
}

sub ROUND_16_XX()
{ my ($i,$a,$b,$c,$d,$e,$f,$g,$h) = @_;

$code.=<<___;
	mov	`$SZ*(($i+1)&0xf)`(%rsp),$a0
	mov	`$SZ*(($i+14)&0xf)`(%rsp),$T1

	mov	$a0,$a2

	shr	\$$sigma0[2],$a0
	ror	\$$sigma0[0],$a2

	xor	$a2,$a0
	ror	\$`$sigma0[1]-$sigma0[0]`,$a2

	xor	$a2,$a0			# sigma0(X[(i+1)&0xf])
	mov	$T1,$a1

	shr	\$$sigma1[2],$T1
	ror	\$$sigma1[0],$a1

	xor	$a1,$T1
	ror	\$`$sigma1[1]-$sigma1[0]`,$a1

	xor	$a1,$T1			# sigma1(X[(i+14)&0xf])

	add	$a0,$T1

	add	`$SZ*(($i+9)&0xf)`(%rsp),$T1

	add	`$SZ*($i&0xf)`(%rsp),$T1
___
	&ROUND_00_15(@_);
}

$code=<<___;
.text

.globl	$func
.type	$func,\@function,4
.align	16
$func:
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	mov	%rsp,%rbp		# copy %rsp
	shl	\$4,%rdx		# num*16
	sub	\$$framesz,%rsp
	lea	($inp,%rdx,$SZ),%rdx	# inp+num*16*$SZ
	and	\$-64,%rsp		# align stack frame
	mov	$ctx,$_ctx		# save ctx, 1st arg
	mov	$inp,$_inp		# save inp, 2nd arh
	mov	%rdx,$_end		# save end pointer, "3rd" arg
	mov	%rbp,$_rsp		# save copy of %rsp

	.picmeup $Tbl
	lea	$TABLE-.($Tbl),$Tbl

	mov	$SZ*0($ctx),$A
	mov	$SZ*1($ctx),$B
	mov	$SZ*2($ctx),$C
	mov	$SZ*3($ctx),$D
	mov	$SZ*4($ctx),$E
	mov	$SZ*5($ctx),$F
	mov	$SZ*6($ctx),$G
	mov	$SZ*7($ctx),$H
	jmp	.Lloop

.align	16
.Lloop:
	xor	$round,$round
___
	for($i=0;$i<16;$i++) {
		$code.="	mov	$SZ*$i($inp),$T1\n";
		$code.="	bswap	$T1\n";
		&ROUND_00_15($i,@ROT);
		unshift(@ROT,pop(@ROT));
	}
$code.=<<___;
	jmp	.Lrounds_16_xx
.align	16
.Lrounds_16_xx:
___
	for(;$i<32;$i++) {
		&ROUND_16_XX($i,@ROT);
		unshift(@ROT,pop(@ROT));
	}

$code.=<<___;
	cmp	\$$rounds,$round
	jb	.Lrounds_16_xx

	mov	$_ctx,$ctx
	lea	16*$SZ($inp),$inp

	add	$SZ*0($ctx),$A
	add	$SZ*1($ctx),$B
	add	$SZ*2($ctx),$C
	add	$SZ*3($ctx),$D
	add	$SZ*4($ctx),$E
	add	$SZ*5($ctx),$F
	add	$SZ*6($ctx),$G
	add	$SZ*7($ctx),$H

	cmp	$_end,$inp

	mov	$A,$SZ*0($ctx)
	mov	$B,$SZ*1($ctx)
	mov	$C,$SZ*2($ctx)
	mov	$D,$SZ*3($ctx)
	mov	$E,$SZ*4($ctx)
	mov	$F,$SZ*5($ctx)
	mov	$G,$SZ*6($ctx)
	mov	$H,$SZ*7($ctx)
	jb	.Lloop

	mov	$_rsp,%rsp
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx

	ret
.size	$func,.-$func
___

if ($SZ==4) {
$code.=<<___;
.align	64
.type	$TABLE,\@object
$TABLE:
	.long	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5
	.long	0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5
	.long	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3
	.long	0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174
	.long	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc
	.long	0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da
	.long	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7
	.long	0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967
	.long	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13
	.long	0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85
	.long	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3
	.long	0xd192e819,0xd6990624,0xf40e3585,0x106aa070
	.long	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5
	.long	0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3
	.long	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208
	.long	0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
___
} else {
$code.=<<___;
.align	64
.type	$TABLE,\@object
$TABLE:
	.quad	0x428a2f98d728ae22,0x7137449123ef65cd
	.quad	0xb5c0fbcfec4d3b2f,0xe9b5dba58189dbbc
	.quad	0x3956c25bf348b538,0x59f111f1b605d019
	.quad	0x923f82a4af194f9b,0xab1c5ed5da6d8118
	.quad	0xd807aa98a3030242,0x12835b0145706fbe
	.quad	0x243185be4ee4b28c,0x550c7dc3d5ffb4e2
	.quad	0x72be5d74f27b896f,0x80deb1fe3b1696b1
	.quad	0x9bdc06a725c71235,0xc19bf174cf692694
	.quad	0xe49b69c19ef14ad2,0xefbe4786384f25e3
	.quad	0x0fc19dc68b8cd5b5,0x240ca1cc77ac9c65
	.quad	0x2de92c6f592b0275,0x4a7484aa6ea6e483
	.quad	0x5cb0a9dcbd41fbd4,0x76f988da831153b5
	.quad	0x983e5152ee66dfab,0xa831c66d2db43210
	.quad	0xb00327c898fb213f,0xbf597fc7beef0ee4
	.quad	0xc6e00bf33da88fc2,0xd5a79147930aa725
	.quad	0x06ca6351e003826f,0x142929670a0e6e70
	.quad	0x27b70a8546d22ffc,0x2e1b21385c26c926
	.quad	0x4d2c6dfc5ac42aed,0x53380d139d95b3df
	.quad	0x650a73548baf63de,0x766a0abb3c77b2a8
	.quad	0x81c2c92e47edaee6,0x92722c851482353b
	.quad	0xa2bfe8a14cf10364,0xa81a664bbc423001
	.quad	0xc24b8b70d0f89791,0xc76c51a30654be30
	.quad	0xd192e819d6ef5218,0xd69906245565a910
	.quad	0xf40e35855771202a,0x106aa07032bbd1b8
	.quad	0x19a4c116b8d2d0c8,0x1e376c085141ab53
	.quad	0x2748774cdf8eeb99,0x34b0bcb5e19b48a8
	.quad	0x391c0cb3c5c95a63,0x4ed8aa4ae3418acb
	.quad	0x5b9cca4f7763e373,0x682e6ff3d6b2b8a3
	.quad	0x748f82ee5defb2fc,0x78a5636f43172f60
	.quad	0x84c87814a1f0ab72,0x8cc702081a6439ec
	.quad	0x90befffa23631e28,0xa4506cebde82bde9
	.quad	0xbef9a3f7b2c67915,0xc67178f2e372532b
	.quad	0xca273eceea26619c,0xd186b8c721c0c207
	.quad	0xeada7dd6cde0eb1e,0xf57d4f7fee6ed178
	.quad	0x06f067aa72176fba,0x0a637dc5a2c898a6
	.quad	0x113f9804bef90dae,0x1b710b35131c471b
	.quad	0x28db77f523047d84,0x32caab7b40c72493
	.quad	0x3c9ebe0a15c9bebc,0x431d67c49c100d4c
	.quad	0x4cc5d4becb3e42b6,0x597f299cfc657e2a
	.quad	0x5fcb6fab3ad6faec,0x6c44198c4a475817
___
}

$code =~ s/\`([^\`]*)\`/eval $1/gem;
print $code;
close STDOUT;
