#!/usr/bin/env perl
#
# ====================================================================
# Written by Andy Polyakov <appro@openssl.org> for the OpenSSL
# project. The module is, however, dual licensed under OpenSSL and
# CRYPTOGAMS licenses depending on where you obtain it. For further
# details see http://www.openssl.org/~appro/cryptogams/.
# ====================================================================
#
# June 2011
#
# This is AESNI-CBC+SHA1 "stitch" implementation. The idea, as spelled
# in http://download.intel.com/design/intarch/papers/323686.pdf, is
# that since AESNI-CBC encrypt exhibit *very* low instruction-level
# parallelism, interleaving it with another algorithm would allow to
# utilize processor resources better and achieve better performance.
# SHA1 instruction sequences(*) are taken from sha1-x86_64.pl and
# AESNI code is weaved into it. Below are performance numbers in
# cycles per processed byte, less is better, for standalone AESNI-CBC
# encrypt, sum of the latter and standalone SHA1, and "stitched"
# subroutine:
#
#		AES-128-CBC	+SHA1		stitch      gain
# Westmere	3.77[+5.6]	9.37		6.65	    +41%
# Sandy Bridge	5.05[+5.2(6.3)]	10.25(11.35)	6.16(7.08)  +67%(+60%)
#
#		AES-192-CBC
# Westmere	4.51		10.11		6.97	    +45%
# Sandy Bridge	6.05		11.25(12.35)	6.34(7.27)  +77%(+70%)
#
#		AES-256-CBC
# Westmere	5.25		10.85		7.25	    +50%
# Sandy Bridge	7.05		12.25(13.35)	7.06(7.70)  +74%(+73%)
#
# (*)	There are two code paths: SSSE3 and AVX. See sha1-568.pl for
#	background information. Above numbers in parentheses are SSSE3
#	results collected on AVX-capable CPU, i.e. apply on OSes that
#	don't support AVX.
#
# Needless to mention that it makes no sense to implement "stitched"
# *decrypt* subroutine. Because *both* AESNI-CBC decrypt and SHA1
# fully utilize parallelism, so stitching would not give any gain
# anyway. Well, there might be some, e.g. because of better cache
# locality... For reference, here are performance results for
# standalone AESNI-CBC decrypt:
#
#		AES-128-CBC	AES-192-CBC	AES-256-CBC
# Westmere	1.31		1.55		1.80
# Sandy Bridge	0.93		1.06		1.22

$flavour = shift;
$output  = shift;
if ($flavour =~ /\./) { $output = $flavour; undef $flavour; }

$win64=0; $win64=1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../perlasm/x86_64-xlate.pl" and -f $xlate) or
die "can't locate x86_64-xlate.pl";

$avx=1 if (`$ENV{CC} -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1`
		=~ /GNU assembler version ([2-9]\.[0-9]+)/ &&
	   $1>=2.19);
$avx=1 if (!$avx && $win64 && ($flavour =~ /nasm/ || $ENV{ASM} =~ /nasm/) &&
	   `nasm -v 2>&1` =~ /NASM version ([2-9]\.[0-9]+)/ &&
	   $1>=2.09);
$avx=1 if (!$avx && $win64 && ($flavour =~ /masm/ || $ENV{ASM} =~ /ml64/) &&
	   `ml64 2>&1` =~ /Version ([0-9]+)\./ &&
	   $1>=10);

open OUT,"| \"$^X\" $xlate $flavour $output";
*STDOUT=*OUT;

# void aesni_cbc_sha1_enc(const void *inp,
#			void *out,
#			size_t length,
#			const AES_KEY *key,
#			unsigned char *iv,
#			SHA_CTX *ctx,
#			const void *in0);

$code.=<<___;
.text
.extern	OPENSSL_ia32cap_P

.globl	aesni_cbc_sha1_enc
.type	aesni_cbc_sha1_enc,\@abi-omnipotent
.align	16
aesni_cbc_sha1_enc:
	# caller should check for SSSE3 and AES-NI bits
	mov	OPENSSL_ia32cap_P+0(%rip),%r10d
	mov	OPENSSL_ia32cap_P+4(%rip),%r11d
___
$code.=<<___ if ($avx);
	and	\$`1<<28`,%r11d		# mask AVX bit
	and	\$`1<<30`,%r10d		# mask "Intel CPU" bit
	or	%r11d,%r10d
	cmp	\$`1<<28|1<<30`,%r10d
	je	aesni_cbc_sha1_enc_avx
___
$code.=<<___;
	jmp	aesni_cbc_sha1_enc_ssse3
	ret
.size	aesni_cbc_sha1_enc,.-aesni_cbc_sha1_enc
___

my ($in0,$out,$len,$key,$ivp,$ctx,$inp)=("%rdi","%rsi","%rdx","%rcx","%r8","%r9","%r10");

my $Xi=4;
my @X=map("%xmm$_",(4..7,0..3));
my @Tx=map("%xmm$_",(8..10));
my @V=($A,$B,$C,$D,$E)=("%eax","%ebx","%ecx","%edx","%ebp");	# size optimization
my @T=("%esi","%edi");
my $j=0; my $jj=0; my $r=0; my $sn=0;
my $K_XX_XX="%r11";
my ($iv,$in,$rndkey0)=map("%xmm$_",(11..13));
my @rndkey=("%xmm14","%xmm15");

sub AUTOLOAD()		# thunk [simplified] 32-bit style perlasm
{ my $opcode = $AUTOLOAD; $opcode =~ s/.*:://;
  my $arg = pop;
    $arg = "\$$arg" if ($arg*1 eq $arg);
    $code .= "\t$opcode\t".join(',',$arg,reverse @_)."\n";
}

my $_rol=sub { &rol(@_) };
my $_ror=sub { &ror(@_) };

$code.=<<___;
.type	aesni_cbc_sha1_enc_ssse3,\@function,6
.align	16
aesni_cbc_sha1_enc_ssse3:
	mov	`($win64?56:8)`(%rsp),$inp	# load 7th argument
	#shr	\$6,$len			# debugging artefact
	#jz	.Lepilogue_ssse3		# debugging artefact
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	lea	`-104-($win64?10*16:0)`(%rsp),%rsp
	#mov	$in0,$inp			# debugging artefact
	#lea	64(%rsp),$ctx			# debugging artefact
___
$code.=<<___ if ($win64);
	movaps	%xmm6,96+0(%rsp)
	movaps	%xmm7,96+16(%rsp)
	movaps	%xmm8,96+32(%rsp)
	movaps	%xmm9,96+48(%rsp)
	movaps	%xmm10,96+64(%rsp)
	movaps	%xmm11,96+80(%rsp)
	movaps	%xmm12,96+96(%rsp)
	movaps	%xmm13,96+112(%rsp)
	movaps	%xmm14,96+128(%rsp)
	movaps	%xmm15,96+144(%rsp)
.Lprologue_ssse3:
___
$code.=<<___;
	mov	$in0,%r12			# reassign arguments
	mov	$out,%r13
	mov	$len,%r14
	mov	$key,%r15
	movdqu	($ivp),$iv			# load IV
	mov	$ivp,88(%rsp)			# save $ivp
___
my ($in0,$out,$len,$key)=map("%r$_",(12..15));	# reassign arguments
my $rounds="${ivp}d";
$code.=<<___;
	shl	\$6,$len
	sub	$in0,$out
	mov	240($key),$rounds
	add	$inp,$len		# end of input

	lea	K_XX_XX(%rip),$K_XX_XX
	mov	0($ctx),$A		# load context
	mov	4($ctx),$B
	mov	8($ctx),$C
	mov	12($ctx),$D
	mov	$B,@T[0]		# magic seed
	mov	16($ctx),$E

	movdqa	64($K_XX_XX),@X[2]	# pbswap mask
	movdqa	0($K_XX_XX),@Tx[1]	# K_00_19
	movdqu	0($inp),@X[-4&7]	# load input to %xmm[0-3]
	movdqu	16($inp),@X[-3&7]
	movdqu	32($inp),@X[-2&7]
	movdqu	48($inp),@X[-1&7]
	pshufb	@X[2],@X[-4&7]		# byte swap
	add	\$64,$inp
	pshufb	@X[2],@X[-3&7]
	pshufb	@X[2],@X[-2&7]
	pshufb	@X[2],@X[-1&7]
	paddd	@Tx[1],@X[-4&7]		# add K_00_19
	paddd	@Tx[1],@X[-3&7]
	paddd	@Tx[1],@X[-2&7]
	movdqa	@X[-4&7],0(%rsp)	# X[]+K xfer to IALU
	psubd	@Tx[1],@X[-4&7]		# restore X[]
	movdqa	@X[-3&7],16(%rsp)
	psubd	@Tx[1],@X[-3&7]
	movdqa	@X[-2&7],32(%rsp)
	psubd	@Tx[1],@X[-2&7]
	movups	($key),$rndkey0		# $key[0]
	movups	16($key),$rndkey[0]	# forward reference
	jmp	.Loop_ssse3
___

my $aesenc=sub {
  use integer;
  my ($n,$k)=($r/10,$r%10);
    if ($k==0) {
      $code.=<<___;
	movups		`16*$n`($in0),$in		# load input
	xorps		$rndkey0,$in
___
      $code.=<<___ if ($n);
	movups		$iv,`16*($n-1)`($out,$in0)	# write output
___
      $code.=<<___;
	xorps		$in,$iv
	aesenc		$rndkey[0],$iv
	movups		`32+16*$k`($key),$rndkey[1]
___
    } elsif ($k==9) {
      $sn++;
      $code.=<<___;
	cmp		\$11,$rounds
	jb		.Laesenclast$sn
	movups		`32+16*($k+0)`($key),$rndkey[1]
	aesenc		$rndkey[0],$iv
	movups		`32+16*($k+1)`($key),$rndkey[0]
	aesenc		$rndkey[1],$iv
	je		.Laesenclast$sn
	movups		`32+16*($k+2)`($key),$rndkey[1]
	aesenc		$rndkey[0],$iv
	movups		`32+16*($k+3)`($key),$rndkey[0]
	aesenc		$rndkey[1],$iv
.Laesenclast$sn:
	aesenclast	$rndkey[0],$iv
	movups		16($key),$rndkey[1]		# forward reference
___
    } else {
      $code.=<<___;
	aesenc		$rndkey[0],$iv
	movups		`32+16*$k`($key),$rndkey[1]
___
    }
    $r++;	unshift(@rndkey,pop(@rndkey));
};

sub Xupdate_ssse3_16_31()		# recall that $Xi starts wtih 4
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	# 40 instructions
  my ($a,$b,$c,$d,$e);

	&movdqa	(@X[0],@X[-3&7]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&movdqa	(@Tx[0],@X[-1&7]);
	&palignr(@X[0],@X[-4&7],8);	# compose "X[-14]" in "X[0]"
	 eval(shift(@insns));
	 eval(shift(@insns));

	  &paddd	(@Tx[1],@X[-1&7]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&psrldq	(@Tx[0],4);		# "X[-3]", 3 dwords
	 eval(shift(@insns));
	 eval(shift(@insns));
	&pxor	(@X[0],@X[-4&7]);	# "X[0]"^="X[-16]"
	 eval(shift(@insns));
	 eval(shift(@insns));

	&pxor	(@Tx[0],@X[-2&7]);	# "X[-3]"^"X[-8]"
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&pxor	(@X[0],@Tx[0]);		# "X[0]"^="X[-3]"^"X[-8]"
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &movdqa	(eval(16*(($Xi-1)&3))."(%rsp)",@Tx[1]);	# X[]+K xfer to IALU
	 eval(shift(@insns));
	 eval(shift(@insns));

	&movdqa	(@Tx[2],@X[0]);
	&movdqa	(@Tx[0],@X[0]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&pslldq	(@Tx[2],12);		# "X[0]"<<96, extract one dword
	&paddd	(@X[0],@X[0]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&psrld	(@Tx[0],31);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&movdqa	(@Tx[1],@Tx[2]);
	 eval(shift(@insns));
	 eval(shift(@insns));

	&psrld	(@Tx[2],30);
	&por	(@X[0],@Tx[0]);		# "X[0]"<<<=1
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&pslld	(@Tx[1],2);
	&pxor	(@X[0],@Tx[2]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &movdqa	(@Tx[2],eval(16*(($Xi)/5))."($K_XX_XX)");	# K_XX_XX
	 eval(shift(@insns));
	 eval(shift(@insns));

	&pxor	(@X[0],@Tx[1]);		# "X[0]"^=("X[0]">>96)<<<2

	 foreach (@insns) { eval; }	# remaining instructions [if any]

  $Xi++;	push(@X,shift(@X));	# "rotate" X[]
		push(@Tx,shift(@Tx));
}

sub Xupdate_ssse3_32_79()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	# 32 to 48 instructions
  my ($a,$b,$c,$d,$e);

	&movdqa	(@Tx[0],@X[-1&7])	if ($Xi==8);
	 eval(shift(@insns));		# body_20_39
	&pxor	(@X[0],@X[-4&7]);	# "X[0]"="X[-32]"^"X[-16]"
	&palignr(@Tx[0],@X[-2&7],8);	# compose "X[-6]"
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		# rol

	&pxor	(@X[0],@X[-7&7]);	# "X[0]"^="X[-28]"
	 eval(shift(@insns));
	 eval(shift(@insns))	if (@insns[0] !~ /&ro[rl]/);
	if ($Xi%5) {
	  &movdqa	(@Tx[2],@Tx[1]);# "perpetuate" K_XX_XX...
	} else {			# ... or load next one
	  &movdqa	(@Tx[2],eval(16*($Xi/5))."($K_XX_XX)");
	}
	  &paddd	(@Tx[1],@X[-1&7]);
	 eval(shift(@insns));		# ror
	 eval(shift(@insns));

	&pxor	(@X[0],@Tx[0]);		# "X[0]"^="X[-6]"
	 eval(shift(@insns));		# body_20_39
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		# rol

	&movdqa	(@Tx[0],@X[0]);
	  &movdqa	(eval(16*(($Xi-1)&3))."(%rsp)",@Tx[1]);	# X[]+K xfer to IALU
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		# ror
	 eval(shift(@insns));

	&pslld	(@X[0],2);
	 eval(shift(@insns));		# body_20_39
	 eval(shift(@insns));
	&psrld	(@Tx[0],30);
	 eval(shift(@insns));
	 eval(shift(@insns));		# rol
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		# ror
	 eval(shift(@insns));

	&por	(@X[0],@Tx[0]);		# "X[0]"<<<=2
	 eval(shift(@insns));		# body_20_39
	 eval(shift(@insns));
	  &movdqa	(@Tx[1],@X[0])	if ($Xi<19);
	 eval(shift(@insns));
	 eval(shift(@insns));		# rol
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		# rol
	 eval(shift(@insns));

	 foreach (@insns) { eval; }	# remaining instructions

  $Xi++;	push(@X,shift(@X));	# "rotate" X[]
		push(@Tx,shift(@Tx));
}

sub Xuplast_ssse3_80()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	# 32 instructions
  my ($a,$b,$c,$d,$e);

	 eval(shift(@insns));
	  &paddd	(@Tx[1],@X[-1&7]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	  &movdqa	(eval(16*(($Xi-1)&3))."(%rsp)",@Tx[1]);	# X[]+K xfer IALU

	 foreach (@insns) { eval; }		# remaining instructions

	&cmp	($inp,$len);
	&je	(".Ldone_ssse3");

	unshift(@Tx,pop(@Tx));

	&movdqa	(@X[2],"64($K_XX_XX)");		# pbswap mask
	&movdqa	(@Tx[1],"0($K_XX_XX)");		# K_00_19
	&movdqu	(@X[-4&7],"0($inp)");		# load input
	&movdqu	(@X[-3&7],"16($inp)");
	&movdqu	(@X[-2&7],"32($inp)");
	&movdqu	(@X[-1&7],"48($inp)");
	&pshufb	(@X[-4&7],@X[2]);		# byte swap
	&add	($inp,64);

  $Xi=0;
}

sub Xloop_ssse3()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	# 32 instructions
  my ($a,$b,$c,$d,$e);

	 eval(shift(@insns));
	 eval(shift(@insns));
	&pshufb	(@X[($Xi-3)&7],@X[2]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&paddd	(@X[($Xi-4)&7],@Tx[1]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	&movdqa	(eval(16*$Xi)."(%rsp)",@X[($Xi-4)&7]);	# X[]+K xfer to IALU
	 eval(shift(@insns));
	 eval(shift(@insns));
	&psubd	(@X[($Xi-4)&7],@Tx[1]);

	foreach (@insns) { eval; }
  $Xi++;
}

sub Xtail_ssse3()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	# 32 instructions
  my ($a,$b,$c,$d,$e);

	foreach (@insns) { eval; }
}

sub body_00_19 () {
  use integer;
  my ($k,$n);
  my @r=(
	'($a,$b,$c,$d,$e)=@V;'.
	'&add	($e,eval(4*($j&15))."(%rsp)");',	# X[]+K xfer
	'&xor	($c,$d);',
	'&mov	(@T[1],$a);',	# $b in next round
	'&$_rol	($a,5);',
	'&and	(@T[0],$c);',	# ($b&($c^$d))
	'&xor	($c,$d);',	# restore $c
	'&xor	(@T[0],$d);',
	'&add	($e,$a);',
	'&$_ror	($b,$j?7:2);',	# $b>>>2
	'&add	($e,@T[0]);'	.'$j++; unshift(@V,pop(@V)); unshift(@T,pop(@T));'
	);
	$n = scalar(@r);
	$k = (($jj+1)*12/20)*20*$n/12;	# 12 aesencs per these 20 rounds
	@r[$k%$n].='&$aesenc();'	if ($jj==$k/$n);
	$jj++;
    return @r;
}

sub body_20_39 () {
  use integer;
  my ($k,$n);
  my @r=(
	'($a,$b,$c,$d,$e)=@V;'.
	'&add	($e,eval(4*($j++&15))."(%rsp)");',	# X[]+K xfer
	'&xor	(@T[0],$d);',	# ($b^$d)
	'&mov	(@T[1],$a);',	# $b in next round
	'&$_rol	($a,5);',
	'&xor	(@T[0],$c);',	# ($b^$d^$c)
	'&add	($e,$a);',
	'&$_ror	($b,7);',	# $b>>>2
	'&add	($e,@T[0]);'	.'unshift(@V,pop(@V)); unshift(@T,pop(@T));'
	);
	$n = scalar(@r);
	$k = (($jj+1)*8/20)*20*$n/8;	# 8 aesencs per these 20 rounds
	@r[$k%$n].='&$aesenc();'	if ($jj==$k/$n);
	$jj++;
    return @r;
}

sub body_40_59 () {
  use integer;
  my ($k,$n);
  my @r=(
	'($a,$b,$c,$d,$e)=@V;'.
	'&mov	(@T[1],$c);',
	'&xor	($c,$d);',
	'&add	($e,eval(4*($j++&15))."(%rsp)");',	# X[]+K xfer
	'&and	(@T[1],$d);',
	'&and	(@T[0],$c);',	# ($b&($c^$d))
	'&$_ror	($b,7);',	# $b>>>2
	'&add	($e,@T[1]);',
	'&mov	(@T[1],$a);',	# $b in next round
	'&$_rol	($a,5);',
	'&add	($e,@T[0]);',
	'&xor	($c,$d);',	# restore $c
	'&add	($e,$a);'	.'unshift(@V,pop(@V)); unshift(@T,pop(@T));'
	);
	$n = scalar(@r);
	$k=(($jj+1)*12/20)*20*$n/12;	# 12 aesencs per these 20 rounds
	@r[$k%$n].='&$aesenc();'	if ($jj==$k/$n);
	$jj++;
    return @r;
}
$code.=<<___;
.align	16
.Loop_ssse3:
___
	&Xupdate_ssse3_16_31(\&body_00_19);
	&Xupdate_ssse3_16_31(\&body_00_19);
	&Xupdate_ssse3_16_31(\&body_00_19);
	&Xupdate_ssse3_16_31(\&body_00_19);
	&Xupdate_ssse3_32_79(\&body_00_19);
	&Xupdate_ssse3_32_79(\&body_20_39);
	&Xupdate_ssse3_32_79(\&body_20_39);
	&Xupdate_ssse3_32_79(\&body_20_39);
	&Xupdate_ssse3_32_79(\&body_20_39);
	&Xupdate_ssse3_32_79(\&body_20_39);
	&Xupdate_ssse3_32_79(\&body_40_59);
	&Xupdate_ssse3_32_79(\&body_40_59);
	&Xupdate_ssse3_32_79(\&body_40_59);
	&Xupdate_ssse3_32_79(\&body_40_59);
	&Xupdate_ssse3_32_79(\&body_40_59);
	&Xupdate_ssse3_32_79(\&body_20_39);
	&Xuplast_ssse3_80(\&body_20_39);	# can jump to "done"

				$saved_j=$j; @saved_V=@V;
				$saved_r=$r; @saved_rndkey=@rndkey;

	&Xloop_ssse3(\&body_20_39);
	&Xloop_ssse3(\&body_20_39);
	&Xloop_ssse3(\&body_20_39);

$code.=<<___;
	movups	$iv,48($out,$in0)		# write output
	lea	64($in0),$in0

	add	0($ctx),$A			# update context
	add	4($ctx),@T[0]
	add	8($ctx),$C
	add	12($ctx),$D
	mov	$A,0($ctx)
	add	16($ctx),$E
	mov	@T[0],4($ctx)
	mov	@T[0],$B			# magic seed
	mov	$C,8($ctx)
	mov	$D,12($ctx)
	mov	$E,16($ctx)
	jmp	.Loop_ssse3

.align	16
.Ldone_ssse3:
___
				$jj=$j=$saved_j; @V=@saved_V;
				$r=$saved_r;     @rndkey=@saved_rndkey;

	&Xtail_ssse3(\&body_20_39);
	&Xtail_ssse3(\&body_20_39);
	&Xtail_ssse3(\&body_20_39);

$code.=<<___;
	movups	$iv,48($out,$in0)		# write output
	mov	88(%rsp),$ivp			# restore $ivp

	add	0($ctx),$A			# update context
	add	4($ctx),@T[0]
	add	8($ctx),$C
	mov	$A,0($ctx)
	add	12($ctx),$D
	mov	@T[0],4($ctx)
	add	16($ctx),$E
	mov	$C,8($ctx)
	mov	$D,12($ctx)
	mov	$E,16($ctx)
	movups	$iv,($ivp)			# write IV
___
$code.=<<___ if ($win64);
	movaps	96+0(%rsp),%xmm6
	movaps	96+16(%rsp),%xmm7
	movaps	96+32(%rsp),%xmm8
	movaps	96+48(%rsp),%xmm9
	movaps	96+64(%rsp),%xmm10
	movaps	96+80(%rsp),%xmm11
	movaps	96+96(%rsp),%xmm12
	movaps	96+112(%rsp),%xmm13
	movaps	96+128(%rsp),%xmm14
	movaps	96+144(%rsp),%xmm15
___
$code.=<<___;
	lea	`104+($win64?10*16:0)`(%rsp),%rsi
	mov	0(%rsi),%r15
	mov	8(%rsi),%r14
	mov	16(%rsi),%r13
	mov	24(%rsi),%r12
	mov	32(%rsi),%rbp
	mov	40(%rsi),%rbx
	lea	48(%rsi),%rsp
.Lepilogue_ssse3:
	ret
.size	aesni_cbc_sha1_enc_ssse3,.-aesni_cbc_sha1_enc_ssse3
___

$j=$jj=$r=$sn=0;

if ($avx) {
my ($in0,$out,$len,$key,$ivp,$ctx,$inp)=("%rdi","%rsi","%rdx","%rcx","%r8","%r9","%r10");

my $Xi=4;
my @X=map("%xmm$_",(4..7,0..3));
my @Tx=map("%xmm$_",(8..10));
my @V=($A,$B,$C,$D,$E)=("%eax","%ebx","%ecx","%edx","%ebp");	# size optimization
my @T=("%esi","%edi");

my $_rol=sub { &shld(@_[0],@_) };
my $_ror=sub { &shrd(@_[0],@_) };

$code.=<<___;
.type	aesni_cbc_sha1_enc_avx,\@function,6
.align	16
aesni_cbc_sha1_enc_avx:
	mov	`($win64?56:8)`(%rsp),$inp	# load 7th argument
	#shr	\$6,$len			# debugging artefact
	#jz	.Lepilogue_avx			# debugging artefact
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	lea	`-104-($win64?10*16:0)`(%rsp),%rsp
	#mov	$in0,$inp			# debugging artefact
	#lea	64(%rsp),$ctx			# debugging artefact
___
$code.=<<___ if ($win64);
	movaps	%xmm6,96+0(%rsp)
	movaps	%xmm7,96+16(%rsp)
	movaps	%xmm8,96+32(%rsp)
	movaps	%xmm9,96+48(%rsp)
	movaps	%xmm10,96+64(%rsp)
	movaps	%xmm11,96+80(%rsp)
	movaps	%xmm12,96+96(%rsp)
	movaps	%xmm13,96+112(%rsp)
	movaps	%xmm14,96+128(%rsp)
	movaps	%xmm15,96+144(%rsp)
.Lprologue_avx:
___
$code.=<<___;
	vzeroall
	mov	$in0,%r12			# reassign arguments
	mov	$out,%r13
	mov	$len,%r14
	mov	$key,%r15
	vmovdqu	($ivp),$iv			# load IV
	mov	$ivp,88(%rsp)			# save $ivp
___
my ($in0,$out,$len,$key)=map("%r$_",(12..15));	# reassign arguments
my $rounds="${ivp}d";
$code.=<<___;
	shl	\$6,$len
	sub	$in0,$out
	mov	240($key),$rounds
	add	\$112,$key		# size optimization
	add	$inp,$len		# end of input

	lea	K_XX_XX(%rip),$K_XX_XX
	mov	0($ctx),$A		# load context
	mov	4($ctx),$B
	mov	8($ctx),$C
	mov	12($ctx),$D
	mov	$B,@T[0]		# magic seed
	mov	16($ctx),$E

	vmovdqa	64($K_XX_XX),@X[2]	# pbswap mask
	vmovdqa	0($K_XX_XX),@Tx[1]	# K_00_19
	vmovdqu	0($inp),@X[-4&7]	# load input to %xmm[0-3]
	vmovdqu	16($inp),@X[-3&7]
	vmovdqu	32($inp),@X[-2&7]
	vmovdqu	48($inp),@X[-1&7]
	vpshufb	@X[2],@X[-4&7],@X[-4&7]	# byte swap
	add	\$64,$inp
	vpshufb	@X[2],@X[-3&7],@X[-3&7]
	vpshufb	@X[2],@X[-2&7],@X[-2&7]
	vpshufb	@X[2],@X[-1&7],@X[-1&7]
	vpaddd	@Tx[1],@X[-4&7],@X[0]	# add K_00_19
	vpaddd	@Tx[1],@X[-3&7],@X[1]
	vpaddd	@Tx[1],@X[-2&7],@X[2]
	vmovdqa	@X[0],0(%rsp)		# X[]+K xfer to IALU
	vmovdqa	@X[1],16(%rsp)
	vmovdqa	@X[2],32(%rsp)
	vmovups	-112($key),$rndkey0	# $key[0]
	vmovups	16-112($key),$rndkey[0]	# forward reference
	jmp	.Loop_avx
___

my $aesenc=sub {
  use integer;
  my ($n,$k)=($r/10,$r%10);
    if ($k==0) {
      $code.=<<___;
	vmovups		`16*$n`($in0),$in		# load input
	vxorps		$rndkey0,$in,$in
___
      $code.=<<___ if ($n);
	vmovups		$iv,`16*($n-1)`($out,$in0)	# write output
___
      $code.=<<___;
	vxorps		$in,$iv,$iv
	vaesenc		$rndkey[0],$iv,$iv
	vmovups		`32+16*$k-112`($key),$rndkey[1]
___
    } elsif ($k==9) {
      $sn++;
      $code.=<<___;
	cmp		\$11,$rounds
	jb		.Lvaesenclast$sn
	vaesenc		$rndkey[0],$iv,$iv
	vmovups		`32+16*($k+0)-112`($key),$rndkey[1]
	vaesenc		$rndkey[1],$iv,$iv
	vmovups		`32+16*($k+1)-112`($key),$rndkey[0]
	je		.Lvaesenclast$sn
	vaesenc		$rndkey[0],$iv,$iv
	vmovups		`32+16*($k+2)-112`($key),$rndkey[1]
	vaesenc		$rndkey[1],$iv,$iv
	vmovups		`32+16*($k+3)-112`($key),$rndkey[0]
.Lvaesenclast$sn:
	vaesenclast	$rndkey[0],$iv,$iv
	vmovups		16-112($key),$rndkey[1]		# forward reference
___
    } else {
      $code.=<<___;
	vaesenc		$rndkey[0],$iv,$iv
	vmovups		`32+16*$k-112`($key),$rndkey[1]
___
    }
    $r++;	unshift(@rndkey,pop(@rndkey));
};

sub Xupdate_avx_16_31()		# recall that $Xi starts wtih 4
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	# 40 instructions
  my ($a,$b,$c,$d,$e);

	 eval(shift(@insns));
	 eval(shift(@insns));
	&vpalignr(@X[0],@X[-3&7],@X[-4&7],8);	# compose "X[-14]" in "X[0]"
	 eval(shift(@insns));
	 eval(shift(@insns));

	  &vpaddd	(@Tx[1],@Tx[1],@X[-1&7]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vpsrldq(@Tx[0],@X[-1&7],4);	# "X[-3]", 3 dwords
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vpxor	(@X[0],@X[0],@X[-4&7]);		# "X[0]"^="X[-16]"
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpxor	(@Tx[0],@Tx[0],@X[-2&7]);	# "X[-3]"^"X[-8]"
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpxor	(@X[0],@X[0],@Tx[0]);		# "X[0]"^="X[-3]"^"X[-8]"
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vmovdqa	(eval(16*(($Xi-1)&3))."(%rsp)",@Tx[1]);	# X[]+K xfer to IALU
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpsrld	(@Tx[0],@X[0],31);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpslldq(@Tx[2],@X[0],12);		# "X[0]"<<96, extract one dword
	&vpaddd	(@X[0],@X[0],@X[0]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpsrld	(@Tx[1],@Tx[2],30);
	&vpor	(@X[0],@X[0],@Tx[0]);		# "X[0]"<<<=1
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpslld	(@Tx[2],@Tx[2],2);
	&vpxor	(@X[0],@X[0],@Tx[1]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpxor	(@X[0],@X[0],@Tx[2]);		# "X[0]"^=("X[0]">>96)<<<2
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vmovdqa	(@Tx[2],eval(16*(($Xi)/5))."($K_XX_XX)");	# K_XX_XX
	 eval(shift(@insns));
	 eval(shift(@insns));


	 foreach (@insns) { eval; }	# remaining instructions [if any]

  $Xi++;	push(@X,shift(@X));	# "rotate" X[]
		push(@Tx,shift(@Tx));
}

sub Xupdate_avx_32_79()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	# 32 to 48 instructions
  my ($a,$b,$c,$d,$e);

	&vpalignr(@Tx[0],@X[-1&7],@X[-2&7],8);	# compose "X[-6]"
	&vpxor	(@X[0],@X[0],@X[-4&7]);		# "X[0]"="X[-32]"^"X[-16]"
	 eval(shift(@insns));		# body_20_39
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		# rol

	&vpxor	(@X[0],@X[0],@X[-7&7]);		# "X[0]"^="X[-28]"
	 eval(shift(@insns));
	 eval(shift(@insns))	if (@insns[0] !~ /&ro[rl]/);
	if ($Xi%5) {
	  &vmovdqa	(@Tx[2],@Tx[1]);# "perpetuate" K_XX_XX...
	} else {			# ... or load next one
	  &vmovdqa	(@Tx[2],eval(16*($Xi/5))."($K_XX_XX)");
	}
	  &vpaddd	(@Tx[1],@Tx[1],@X[-1&7]);
	 eval(shift(@insns));		# ror
	 eval(shift(@insns));

	&vpxor	(@X[0],@X[0],@Tx[0]);		# "X[0]"^="X[-6]"
	 eval(shift(@insns));		# body_20_39
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		# rol

	&vpsrld	(@Tx[0],@X[0],30);
	  &vmovdqa	(eval(16*(($Xi-1)&3))."(%rsp)",@Tx[1]);	# X[]+K xfer to IALU
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		# ror
	 eval(shift(@insns));

	&vpslld	(@X[0],@X[0],2);
	 eval(shift(@insns));		# body_20_39
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		# rol
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		# ror
	 eval(shift(@insns));

	&vpor	(@X[0],@X[0],@Tx[0]);		# "X[0]"<<<=2
	 eval(shift(@insns));		# body_20_39
	 eval(shift(@insns));
	  &vmovdqa	(@Tx[1],@X[0])	if ($Xi<19);
	 eval(shift(@insns));
	 eval(shift(@insns));		# rol
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		# rol
	 eval(shift(@insns));

	 foreach (@insns) { eval; }	# remaining instructions

  $Xi++;	push(@X,shift(@X));	# "rotate" X[]
		push(@Tx,shift(@Tx));
}

sub Xuplast_avx_80()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	# 32 instructions
  my ($a,$b,$c,$d,$e);

	 eval(shift(@insns));
	  &vpaddd	(@Tx[1],@Tx[1],@X[-1&7]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	  &movdqa	(eval(16*(($Xi-1)&3))."(%rsp)",@Tx[1]);	# X[]+K xfer IALU

	 foreach (@insns) { eval; }		# remaining instructions

	&cmp	($inp,$len);
	&je	(".Ldone_avx");

	unshift(@Tx,pop(@Tx));

	&vmovdqa(@X[2],"64($K_XX_XX)");		# pbswap mask
	&vmovdqa(@Tx[1],"0($K_XX_XX)");		# K_00_19
	&vmovdqu(@X[-4&7],"0($inp)");		# load input
	&vmovdqu(@X[-3&7],"16($inp)");
	&vmovdqu(@X[-2&7],"32($inp)");
	&vmovdqu(@X[-1&7],"48($inp)");
	&vpshufb(@X[-4&7],@X[-4&7],@X[2]);	# byte swap
	&add	($inp,64);

  $Xi=0;
}

sub Xloop_avx()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	# 32 instructions
  my ($a,$b,$c,$d,$e);

	 eval(shift(@insns));
	 eval(shift(@insns));
	&vpshufb(@X[($Xi-3)&7],@X[($Xi-3)&7],@X[2]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vpaddd	(@X[$Xi&7],@X[($Xi-4)&7],@Tx[1]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vmovdqa(eval(16*$Xi)."(%rsp)",@X[$Xi&7]);	# X[]+K xfer to IALU
	 eval(shift(@insns));
	 eval(shift(@insns));

	foreach (@insns) { eval; }
  $Xi++;
}

sub Xtail_avx()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	# 32 instructions
  my ($a,$b,$c,$d,$e);

	foreach (@insns) { eval; }
}

$code.=<<___;
.align	16
.Loop_avx:
___
	&Xupdate_avx_16_31(\&body_00_19);
	&Xupdate_avx_16_31(\&body_00_19);
	&Xupdate_avx_16_31(\&body_00_19);
	&Xupdate_avx_16_31(\&body_00_19);
	&Xupdate_avx_32_79(\&body_00_19);
	&Xupdate_avx_32_79(\&body_20_39);
	&Xupdate_avx_32_79(\&body_20_39);
	&Xupdate_avx_32_79(\&body_20_39);
	&Xupdate_avx_32_79(\&body_20_39);
	&Xupdate_avx_32_79(\&body_20_39);
	&Xupdate_avx_32_79(\&body_40_59);
	&Xupdate_avx_32_79(\&body_40_59);
	&Xupdate_avx_32_79(\&body_40_59);
	&Xupdate_avx_32_79(\&body_40_59);
	&Xupdate_avx_32_79(\&body_40_59);
	&Xupdate_avx_32_79(\&body_20_39);
	&Xuplast_avx_80(\&body_20_39);	# can jump to "done"

				$saved_j=$j; @saved_V=@V;
				$saved_r=$r; @saved_rndkey=@rndkey;

	&Xloop_avx(\&body_20_39);
	&Xloop_avx(\&body_20_39);
	&Xloop_avx(\&body_20_39);

$code.=<<___;
	vmovups	$iv,48($out,$in0)		# write output
	lea	64($in0),$in0

	add	0($ctx),$A			# update context
	add	4($ctx),@T[0]
	add	8($ctx),$C
	add	12($ctx),$D
	mov	$A,0($ctx)
	add	16($ctx),$E
	mov	@T[0],4($ctx)
	mov	@T[0],$B			# magic seed
	mov	$C,8($ctx)
	mov	$D,12($ctx)
	mov	$E,16($ctx)
	jmp	.Loop_avx

.align	16
.Ldone_avx:
___
				$jj=$j=$saved_j; @V=@saved_V;
				$r=$saved_r;     @rndkey=@saved_rndkey;

	&Xtail_avx(\&body_20_39);
	&Xtail_avx(\&body_20_39);
	&Xtail_avx(\&body_20_39);

$code.=<<___;
	vmovups	$iv,48($out,$in0)		# write output
	mov	88(%rsp),$ivp			# restore $ivp

	add	0($ctx),$A			# update context
	add	4($ctx),@T[0]
	add	8($ctx),$C
	mov	$A,0($ctx)
	add	12($ctx),$D
	mov	@T[0],4($ctx)
	add	16($ctx),$E
	mov	$C,8($ctx)
	mov	$D,12($ctx)
	mov	$E,16($ctx)
	vmovups	$iv,($ivp)			# write IV
	vzeroall
___
$code.=<<___ if ($win64);
	movaps	96+0(%rsp),%xmm6
	movaps	96+16(%rsp),%xmm7
	movaps	96+32(%rsp),%xmm8
	movaps	96+48(%rsp),%xmm9
	movaps	96+64(%rsp),%xmm10
	movaps	96+80(%rsp),%xmm11
	movaps	96+96(%rsp),%xmm12
	movaps	96+112(%rsp),%xmm13
	movaps	96+128(%rsp),%xmm14
	movaps	96+144(%rsp),%xmm15
___
$code.=<<___;
	lea	`104+($win64?10*16:0)`(%rsp),%rsi
	mov	0(%rsi),%r15
	mov	8(%rsi),%r14
	mov	16(%rsi),%r13
	mov	24(%rsi),%r12
	mov	32(%rsi),%rbp
	mov	40(%rsi),%rbx
	lea	48(%rsi),%rsp
.Lepilogue_avx:
	ret
.size	aesni_cbc_sha1_enc_avx,.-aesni_cbc_sha1_enc_avx
___
}
$code.=<<___;
.align	64
K_XX_XX:
.long	0x5a827999,0x5a827999,0x5a827999,0x5a827999	# K_00_19
.long	0x6ed9eba1,0x6ed9eba1,0x6ed9eba1,0x6ed9eba1	# K_20_39
.long	0x8f1bbcdc,0x8f1bbcdc,0x8f1bbcdc,0x8f1bbcdc	# K_40_59
.long	0xca62c1d6,0xca62c1d6,0xca62c1d6,0xca62c1d6	# K_60_79
.long	0x00010203,0x04050607,0x08090a0b,0x0c0d0e0f	# pbswap mask

.asciz	"AESNI-CBC+SHA1 stitch for x86_64, CRYPTOGAMS by <appro\@openssl.org>"
.align	64
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
.type	ssse3_handler,\@abi-omnipotent
.align	16
ssse3_handler:
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

	mov	8($disp),%rsi		# disp->ImageBase
	mov	56($disp),%r11		# disp->HandlerData

	mov	0(%r11),%r10d		# HandlerData[0]
	lea	(%rsi,%r10),%r10	# prologue label
	cmp	%r10,%rbx		# context->Rip<prologue label
	jb	.Lcommon_seh_tail

	mov	152($context),%rax	# pull context->Rsp

	mov	4(%r11),%r10d		# HandlerData[1]
	lea	(%rsi,%r10),%r10	# epilogue label
	cmp	%r10,%rbx		# context->Rip>=epilogue label
	jae	.Lcommon_seh_tail

	lea	96(%rax),%rsi
	lea	512($context),%rdi	# &context.Xmm6
	mov	\$20,%ecx
	.long	0xa548f3fc		# cld; rep movsq
	lea	`104+10*16`(%rax),%rax	# adjust stack pointer

	mov	0(%rax),%r15
	mov	8(%rax),%r14
	mov	16(%rax),%r13
	mov	24(%rax),%r12
	mov	32(%rax),%rbp
	mov	40(%rax),%rbx
	lea	48(%rax),%rax
	mov	%rbx,144($context)	# restore context->Rbx
	mov	%rbp,160($context)	# restore context->Rbp
	mov	%r12,216($context)	# restore context->R12
	mov	%r13,224($context)	# restore context->R13
	mov	%r14,232($context)	# restore context->R14
	mov	%r15,240($context)	# restore context->R15

.Lcommon_seh_tail:
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
.size	ssse3_handler,.-ssse3_handler

.section	.pdata
.align	4
	.rva	.LSEH_begin_aesni_cbc_sha1_enc_ssse3
	.rva	.LSEH_end_aesni_cbc_sha1_enc_ssse3
	.rva	.LSEH_info_aesni_cbc_sha1_enc_ssse3
___
$code.=<<___ if ($avx);
	.rva	.LSEH_begin_aesni_cbc_sha1_enc_avx
	.rva	.LSEH_end_aesni_cbc_sha1_enc_avx
	.rva	.LSEH_info_aesni_cbc_sha1_enc_avx
___
$code.=<<___;
.section	.xdata
.align	8
.LSEH_info_aesni_cbc_sha1_enc_ssse3:
	.byte	9,0,0,0
	.rva	ssse3_handler
	.rva	.Lprologue_ssse3,.Lepilogue_ssse3	# HandlerData[]
___
$code.=<<___ if ($avx);
.LSEH_info_aesni_cbc_sha1_enc_avx:
	.byte	9,0,0,0
	.rva	ssse3_handler
	.rva	.Lprologue_avx,.Lepilogue_avx		# HandlerData[]
___
}

####################################################################
sub rex {
  local *opcode=shift;
  my ($dst,$src)=@_;
  my $rex=0;

    $rex|=0x04			if($dst>=8);
    $rex|=0x01			if($src>=8);
    push @opcode,$rex|0x40	if($rex);
}

sub aesni {
  my $line=shift;
  my @opcode=(0x66);

    if ($line=~/(aes[a-z]+)\s+%xmm([0-9]+),\s*%xmm([0-9]+)/) {
	my %opcodelet = (
		"aesenc" => 0xdc,	"aesenclast" => 0xdd
	);
	return undef if (!defined($opcodelet{$1}));
	rex(\@opcode,$3,$2);
	push @opcode,0x0f,0x38,$opcodelet{$1};
	push @opcode,0xc0|($2&7)|(($3&7)<<3);	# ModR/M
	return ".byte\t".join(',',@opcode);
    }
    return $line;
}

$code =~ s/\`([^\`]*)\`/eval($1)/gem;
$code =~ s/\b(aes.*%xmm[0-9]+).*$/aesni($1)/gem;

print $code;
close STDOUT;
