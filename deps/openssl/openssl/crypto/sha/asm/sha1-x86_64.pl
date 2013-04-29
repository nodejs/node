#!/usr/bin/env perl
#
# ====================================================================
# Written by Andy Polyakov <appro@fy.chalmers.se> for the OpenSSL
# project. The module is, however, dual licensed under OpenSSL and
# CRYPTOGAMS licenses depending on where you obtain it. For further
# details see http://www.openssl.org/~appro/cryptogams/.
# ====================================================================
#
# sha1_block procedure for x86_64.
#
# It was brought to my attention that on EM64T compiler-generated code
# was far behind 32-bit assembler implementation. This is unlike on
# Opteron where compiler-generated code was only 15% behind 32-bit
# assembler, which originally made it hard to motivate the effort.
# There was suggestion to mechanically translate 32-bit code, but I
# dismissed it, reasoning that x86_64 offers enough register bank
# capacity to fully utilize SHA-1 parallelism. Therefore this fresh
# implementation:-) However! While 64-bit code does perform better
# on Opteron, I failed to beat 32-bit assembler on EM64T core. Well,
# x86_64 does offer larger *addressable* bank, but out-of-order core
# reaches for even more registers through dynamic aliasing, and EM64T
# core must have managed to run-time optimize even 32-bit code just as
# good as 64-bit one. Performance improvement is summarized in the
# following table:
#
#		gcc 3.4		32-bit asm	cycles/byte
# Opteron	+45%		+20%		6.8
# Xeon P4	+65%		+0%		9.9
# Core2		+60%		+10%		7.0

# August 2009.
#
# The code was revised to minimize code size and to maximize
# "distance" between instructions producing input to 'lea'
# instruction and the 'lea' instruction itself, which is essential
# for Intel Atom core.

# October 2010.
#
# Add SSSE3, Supplemental[!] SSE3, implementation. The idea behind it
# is to offload message schedule denoted by Wt in NIST specification,
# or Xupdate in OpenSSL source, to SIMD unit. See sha1-586.pl module
# for background and implementation details. The only difference from
# 32-bit code is that 64-bit code doesn't have to spill @X[] elements
# to free temporary registers.

# April 2011.
#
# Add AVX code path. See sha1-586.pl for further information.

######################################################################
# Current performance is summarized in following table. Numbers are
# CPU clock cycles spent to process single byte (less is better).
#
#		x86_64		SSSE3		AVX
# P4		9.8		-
# Opteron	6.6		-
# Core2		6.7		6.1/+10%	-
# Atom		11.0		9.7/+13%	-
# Westmere	7.1		5.6/+27%	-
# Sandy Bridge	7.9		6.3/+25%	5.2/+51%

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

$ctx="%rdi";	# 1st arg
$inp="%rsi";	# 2nd arg
$num="%rdx";	# 3rd arg

# reassign arguments in order to produce more compact code
$ctx="%r8";
$inp="%r9";
$num="%r10";

$t0="%eax";
$t1="%ebx";
$t2="%ecx";
@xi=("%edx","%ebp");
$A="%esi";
$B="%edi";
$C="%r11d";
$D="%r12d";
$E="%r13d";

@V=($A,$B,$C,$D,$E);

sub BODY_00_19 {
my ($i,$a,$b,$c,$d,$e)=@_;
my $j=$i+1;
$code.=<<___ if ($i==0);
	mov	`4*$i`($inp),$xi[0]
	bswap	$xi[0]
	mov	$xi[0],`4*$i`(%rsp)
___
$code.=<<___ if ($i<15);
	mov	$c,$t0
	mov	`4*$j`($inp),$xi[1]
	mov	$a,$t2
	xor	$d,$t0
	bswap	$xi[1]
	rol	\$5,$t2
	lea	0x5a827999($xi[0],$e),$e
	and	$b,$t0
	mov	$xi[1],`4*$j`(%rsp)
	add	$t2,$e
	xor	$d,$t0
	rol	\$30,$b
	add	$t0,$e
___
$code.=<<___ if ($i>=15);
	mov	`4*($j%16)`(%rsp),$xi[1]
	mov	$c,$t0
	mov	$a,$t2
	xor	`4*(($j+2)%16)`(%rsp),$xi[1]
	xor	$d,$t0
	rol	\$5,$t2
	xor	`4*(($j+8)%16)`(%rsp),$xi[1]
	and	$b,$t0
	lea	0x5a827999($xi[0],$e),$e
	xor	`4*(($j+13)%16)`(%rsp),$xi[1]
	xor	$d,$t0
	rol	\$1,$xi[1]
	add	$t2,$e
	rol	\$30,$b
	mov	$xi[1],`4*($j%16)`(%rsp)
	add	$t0,$e
___
unshift(@xi,pop(@xi));
}

sub BODY_20_39 {
my ($i,$a,$b,$c,$d,$e)=@_;
my $j=$i+1;
my $K=($i<40)?0x6ed9eba1:0xca62c1d6;
$code.=<<___ if ($i<79);
	mov	`4*($j%16)`(%rsp),$xi[1]
	mov	$c,$t0
	mov	$a,$t2
	xor	`4*(($j+2)%16)`(%rsp),$xi[1]
	xor	$b,$t0
	rol	\$5,$t2
	lea	$K($xi[0],$e),$e
	xor	`4*(($j+8)%16)`(%rsp),$xi[1]
	xor	$d,$t0
	add	$t2,$e
	xor	`4*(($j+13)%16)`(%rsp),$xi[1]
	rol	\$30,$b
	add	$t0,$e
	rol	\$1,$xi[1]
___
$code.=<<___ if ($i<76);
	mov	$xi[1],`4*($j%16)`(%rsp)
___
$code.=<<___ if ($i==79);
	mov	$c,$t0
	mov	$a,$t2
	xor	$b,$t0
	lea	$K($xi[0],$e),$e
	rol	\$5,$t2
	xor	$d,$t0
	add	$t2,$e
	rol	\$30,$b
	add	$t0,$e
___
unshift(@xi,pop(@xi));
}

sub BODY_40_59 {
my ($i,$a,$b,$c,$d,$e)=@_;
my $j=$i+1;
$code.=<<___;
	mov	`4*($j%16)`(%rsp),$xi[1]
	mov	$c,$t0
	mov	$c,$t1
	xor	`4*(($j+2)%16)`(%rsp),$xi[1]
	and	$d,$t0
	mov	$a,$t2
	xor	`4*(($j+8)%16)`(%rsp),$xi[1]
	xor	$d,$t1
	lea	0x8f1bbcdc($xi[0],$e),$e
	rol	\$5,$t2
	xor	`4*(($j+13)%16)`(%rsp),$xi[1]
	add	$t0,$e
	and	$b,$t1
	rol	\$1,$xi[1]
	add	$t1,$e
	rol	\$30,$b
	mov	$xi[1],`4*($j%16)`(%rsp)
	add	$t2,$e
___
unshift(@xi,pop(@xi));
}

$code.=<<___;
.text
.extern	OPENSSL_ia32cap_P

.globl	sha1_block_data_order
.type	sha1_block_data_order,\@function,3
.align	16
sha1_block_data_order:
	mov	OPENSSL_ia32cap_P+0(%rip),%r9d
	mov	OPENSSL_ia32cap_P+4(%rip),%r8d
	test	\$`1<<9`,%r8d		# check SSSE3 bit
	jz	.Lialu
___
$code.=<<___ if ($avx);
	and	\$`1<<28`,%r8d		# mask AVX bit
	and	\$`1<<30`,%r9d		# mask "Intel CPU" bit
	or	%r9d,%r8d
	cmp	\$`1<<28|1<<30`,%r8d
	je	_avx_shortcut
___
$code.=<<___;
	jmp	_ssse3_shortcut

.align	16
.Lialu:
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	mov	%rsp,%r11
	mov	%rdi,$ctx	# reassigned argument
	sub	\$`8+16*4`,%rsp
	mov	%rsi,$inp	# reassigned argument
	and	\$-64,%rsp
	mov	%rdx,$num	# reassigned argument
	mov	%r11,`16*4`(%rsp)
.Lprologue:

	mov	0($ctx),$A
	mov	4($ctx),$B
	mov	8($ctx),$C
	mov	12($ctx),$D
	mov	16($ctx),$E
	jmp	.Lloop

.align	16
.Lloop:
___
for($i=0;$i<20;$i++)	{ &BODY_00_19($i,@V); unshift(@V,pop(@V)); }
for(;$i<40;$i++)	{ &BODY_20_39($i,@V); unshift(@V,pop(@V)); }
for(;$i<60;$i++)	{ &BODY_40_59($i,@V); unshift(@V,pop(@V)); }
for(;$i<80;$i++)	{ &BODY_20_39($i,@V); unshift(@V,pop(@V)); }
$code.=<<___;
	add	0($ctx),$A
	add	4($ctx),$B
	add	8($ctx),$C
	add	12($ctx),$D
	add	16($ctx),$E
	mov	$A,0($ctx)
	mov	$B,4($ctx)
	mov	$C,8($ctx)
	mov	$D,12($ctx)
	mov	$E,16($ctx)

	sub	\$1,$num
	lea	`16*4`($inp),$inp
	jnz	.Lloop

	mov	`16*4`(%rsp),%rsi
	mov	(%rsi),%r13
	mov	8(%rsi),%r12
	mov	16(%rsi),%rbp
	mov	24(%rsi),%rbx
	lea	32(%rsi),%rsp
.Lepilogue:
	ret
.size	sha1_block_data_order,.-sha1_block_data_order
___
{{{
my $Xi=4;
my @X=map("%xmm$_",(4..7,0..3));
my @Tx=map("%xmm$_",(8..10));
my @V=($A,$B,$C,$D,$E)=("%eax","%ebx","%ecx","%edx","%ebp");	# size optimization
my @T=("%esi","%edi");
my $j=0;
my $K_XX_XX="%r11";

my $_rol=sub { &rol(@_) };
my $_ror=sub { &ror(@_) };

$code.=<<___;
.type	sha1_block_data_order_ssse3,\@function,3
.align	16
sha1_block_data_order_ssse3:
_ssse3_shortcut:
	push	%rbx
	push	%rbp
	push	%r12
	lea	`-64-($win64?5*16:0)`(%rsp),%rsp
___
$code.=<<___ if ($win64);
	movaps	%xmm6,64+0(%rsp)
	movaps	%xmm7,64+16(%rsp)
	movaps	%xmm8,64+32(%rsp)
	movaps	%xmm9,64+48(%rsp)
	movaps	%xmm10,64+64(%rsp)
.Lprologue_ssse3:
___
$code.=<<___;
	mov	%rdi,$ctx	# reassigned argument
	mov	%rsi,$inp	# reassigned argument
	mov	%rdx,$num	# reassigned argument

	shl	\$6,$num
	add	$inp,$num
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
	jmp	.Loop_ssse3
___

sub AUTOLOAD()		# thunk [simplified] 32-bit style perlasm
{ my $opcode = $AUTOLOAD; $opcode =~ s/.*:://;
  my $arg = pop;
    $arg = "\$$arg" if ($arg*1 eq $arg);
    $code .= "\t$opcode\t".join(',',$arg,reverse @_)."\n";
}

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

	&cmp	($inp,$num);
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
	(
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
}

sub body_20_39 () {
	(
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
}

sub body_40_59 () {
	(
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

	&Xloop_ssse3(\&body_20_39);
	&Xloop_ssse3(\&body_20_39);
	&Xloop_ssse3(\&body_20_39);

$code.=<<___;
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
				$j=$saved_j; @V=@saved_V;

	&Xtail_ssse3(\&body_20_39);
	&Xtail_ssse3(\&body_20_39);
	&Xtail_ssse3(\&body_20_39);

$code.=<<___;
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
___
$code.=<<___ if ($win64);
	movaps	64+0(%rsp),%xmm6
	movaps	64+16(%rsp),%xmm7
	movaps	64+32(%rsp),%xmm8
	movaps	64+48(%rsp),%xmm9
	movaps	64+64(%rsp),%xmm10
___
$code.=<<___;
	lea	`64+($win64?5*16:0)`(%rsp),%rsi
	mov	0(%rsi),%r12
	mov	8(%rsi),%rbp
	mov	16(%rsi),%rbx
	lea	24(%rsi),%rsp
.Lepilogue_ssse3:
	ret
.size	sha1_block_data_order_ssse3,.-sha1_block_data_order_ssse3
___

if ($avx) {
my $Xi=4;
my @X=map("%xmm$_",(4..7,0..3));
my @Tx=map("%xmm$_",(8..10));
my @V=($A,$B,$C,$D,$E)=("%eax","%ebx","%ecx","%edx","%ebp");	# size optimization
my @T=("%esi","%edi");
my $j=0;
my $K_XX_XX="%r11";

my $_rol=sub { &shld(@_[0],@_) };
my $_ror=sub { &shrd(@_[0],@_) };

$code.=<<___;
.type	sha1_block_data_order_avx,\@function,3
.align	16
sha1_block_data_order_avx:
_avx_shortcut:
	push	%rbx
	push	%rbp
	push	%r12
	lea	`-64-($win64?5*16:0)`(%rsp),%rsp
___
$code.=<<___ if ($win64);
	movaps	%xmm6,64+0(%rsp)
	movaps	%xmm7,64+16(%rsp)
	movaps	%xmm8,64+32(%rsp)
	movaps	%xmm9,64+48(%rsp)
	movaps	%xmm10,64+64(%rsp)
.Lprologue_avx:
___
$code.=<<___;
	mov	%rdi,$ctx	# reassigned argument
	mov	%rsi,$inp	# reassigned argument
	mov	%rdx,$num	# reassigned argument
	vzeroall

	shl	\$6,$num
	add	$inp,$num
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
	jmp	.Loop_avx
___

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

	&cmp	($inp,$num);
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

	&Xloop_avx(\&body_20_39);
	&Xloop_avx(\&body_20_39);
	&Xloop_avx(\&body_20_39);

$code.=<<___;
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
				$j=$saved_j; @V=@saved_V;

	&Xtail_avx(\&body_20_39);
	&Xtail_avx(\&body_20_39);
	&Xtail_avx(\&body_20_39);

$code.=<<___;
	vzeroall

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
___
$code.=<<___ if ($win64);
	movaps	64+0(%rsp),%xmm6
	movaps	64+16(%rsp),%xmm7
	movaps	64+32(%rsp),%xmm8
	movaps	64+48(%rsp),%xmm9
	movaps	64+64(%rsp),%xmm10
___
$code.=<<___;
	lea	`64+($win64?5*16:0)`(%rsp),%rsi
	mov	0(%rsi),%r12
	mov	8(%rsi),%rbp
	mov	16(%rsi),%rbx
	lea	24(%rsi),%rsp
.Lepilogue_avx:
	ret
.size	sha1_block_data_order_avx,.-sha1_block_data_order_avx
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
___
}}}
$code.=<<___;
.asciz	"SHA1 block transform for x86_64, CRYPTOGAMS by <appro\@openssl.org>"
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
	jb	.Lcommon_seh_tail

	mov	152($context),%rax	# pull context->Rsp

	lea	.Lepilogue(%rip),%r10
	cmp	%r10,%rbx		# context->Rip>=.Lepilogue
	jae	.Lcommon_seh_tail

	mov	`16*4`(%rax),%rax	# pull saved stack pointer
	lea	32(%rax),%rax

	mov	-8(%rax),%rbx
	mov	-16(%rax),%rbp
	mov	-24(%rax),%r12
	mov	-32(%rax),%r13
	mov	%rbx,144($context)	# restore context->Rbx
	mov	%rbp,160($context)	# restore context->Rbp
	mov	%r12,216($context)	# restore context->R12
	mov	%r13,224($context)	# restore context->R13

	jmp	.Lcommon_seh_tail
.size	se_handler,.-se_handler

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

	lea	64(%rax),%rsi
	lea	512($context),%rdi	# &context.Xmm6
	mov	\$10,%ecx
	.long	0xa548f3fc		# cld; rep movsq
	lea	`24+64+5*16`(%rax),%rax	# adjust stack pointer

	mov	-8(%rax),%rbx
	mov	-16(%rax),%rbp
	mov	-24(%rax),%r12
	mov	%rbx,144($context)	# restore context->Rbx
	mov	%rbp,160($context)	# restore context->Rbp
	mov	%r12,216($context)	# restore cotnext->R12

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
	.rva	.LSEH_begin_sha1_block_data_order
	.rva	.LSEH_end_sha1_block_data_order
	.rva	.LSEH_info_sha1_block_data_order
	.rva	.LSEH_begin_sha1_block_data_order_ssse3
	.rva	.LSEH_end_sha1_block_data_order_ssse3
	.rva	.LSEH_info_sha1_block_data_order_ssse3
___
$code.=<<___ if ($avx);
	.rva	.LSEH_begin_sha1_block_data_order_avx
	.rva	.LSEH_end_sha1_block_data_order_avx
	.rva	.LSEH_info_sha1_block_data_order_avx
___
$code.=<<___;
.section	.xdata
.align	8
.LSEH_info_sha1_block_data_order:
	.byte	9,0,0,0
	.rva	se_handler
.LSEH_info_sha1_block_data_order_ssse3:
	.byte	9,0,0,0
	.rva	ssse3_handler
	.rva	.Lprologue_ssse3,.Lepilogue_ssse3	# HandlerData[]
___
$code.=<<___ if ($avx);
.LSEH_info_sha1_block_data_order_avx:
	.byte	9,0,0,0
	.rva	ssse3_handler
	.rva	.Lprologue_avx,.Lepilogue_avx		# HandlerData[]
___
}

####################################################################

$code =~ s/\`([^\`]*)\`/eval $1/gem;
print $code;
close STDOUT;
