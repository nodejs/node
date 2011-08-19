#!/usr/bin/env perl

# ====================================================================
# [Re]written by Andy Polyakov <appro@fy.chalmers.se> for the OpenSSL
# project. The module is, however, dual licensed under OpenSSL and
# CRYPTOGAMS licenses depending on where you obtain it. For further
# details see http://www.openssl.org/~appro/cryptogams/.
# ====================================================================

# "[Re]written" was achieved in two major overhauls. In 2004 BODY_*
# functions were re-implemented to address P4 performance issue [see
# commentary below], and in 2006 the rest was rewritten in order to
# gain freedom to liberate licensing terms.

# It was noted that Intel IA-32 C compiler generates code which
# performs ~30% *faster* on P4 CPU than original *hand-coded*
# SHA1 assembler implementation. To address this problem (and
# prove that humans are still better than machines:-), the
# original code was overhauled, which resulted in following
# performance changes:
#
#		compared with original	compared with Intel cc
#		assembler impl.		generated code
# Pentium	-16%			+48%
# PIII/AMD	+8%			+16%
# P4		+85%(!)			+45%
#
# As you can see Pentium came out as looser:-( Yet I reckoned that
# improvement on P4 outweights the loss and incorporate this
# re-tuned code to 0.9.7 and later.
# ----------------------------------------------------------------
#					<appro@fy.chalmers.se>

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
push(@INC,"${dir}","${dir}../../perlasm");
require "x86asm.pl";

&asm_init($ARGV[0],"sha1-586.pl",$ARGV[$#ARGV] eq "386");

$A="eax";
$B="ebx";
$C="ecx";
$D="edx";
$E="edi";
$T="esi";
$tmp1="ebp";

@V=($A,$B,$C,$D,$E,$T);

sub BODY_00_15
	{
	local($n,$a,$b,$c,$d,$e,$f)=@_;

	&comment("00_15 $n");

	&mov($f,$c);			# f to hold F_00_19(b,c,d)
	 if ($n==0)  { &mov($tmp1,$a); }
	 else        { &mov($a,$tmp1); }
	&rotl($tmp1,5);			# tmp1=ROTATE(a,5)
	 &xor($f,$d);
	&add($tmp1,$e);			# tmp1+=e;
	 &and($f,$b);
	&mov($e,&swtmp($n%16));		# e becomes volatile and is loaded
	 				# with xi, also note that e becomes
					# f in next round...
	 &xor($f,$d);			# f holds F_00_19(b,c,d)
	&rotr($b,2);			# b=ROTATE(b,30)
	 &lea($tmp1,&DWP(0x5a827999,$tmp1,$e));	# tmp1+=K_00_19+xi

	if ($n==15) { &add($f,$tmp1); }	# f+=tmp1
	else        { &add($tmp1,$f); }	# f becomes a in next round
	}

sub BODY_16_19
	{
	local($n,$a,$b,$c,$d,$e,$f)=@_;

	&comment("16_19 $n");

	&mov($f,&swtmp($n%16));		# f to hold Xupdate(xi,xa,xb,xc,xd)
	 &mov($tmp1,$c);		# tmp1 to hold F_00_19(b,c,d)
	&xor($f,&swtmp(($n+2)%16));
	 &xor($tmp1,$d);
	&xor($f,&swtmp(($n+8)%16));
	 &and($tmp1,$b);		# tmp1 holds F_00_19(b,c,d)
	&rotr($b,2);			# b=ROTATE(b,30)
	 &xor($f,&swtmp(($n+13)%16));	# f holds xa^xb^xc^xd
	&rotl($f,1);			# f=ROTATE(f,1)
	 &xor($tmp1,$d);		# tmp1=F_00_19(b,c,d)
	&mov(&swtmp($n%16),$f);		# xi=f
	&lea($f,&DWP(0x5a827999,$f,$e));# f+=K_00_19+e
	 &mov($e,$a);			# e becomes volatile
	&rotl($e,5);			# e=ROTATE(a,5)
	 &add($f,$tmp1);		# f+=F_00_19(b,c,d)
	&add($f,$e);			# f+=ROTATE(a,5)
	}

sub BODY_20_39
	{
	local($n,$a,$b,$c,$d,$e,$f)=@_;
	local $K=($n<40)?0x6ed9eba1:0xca62c1d6;

	&comment("20_39 $n");

	&mov($tmp1,$b);			# tmp1 to hold F_20_39(b,c,d)
	 &mov($f,&swtmp($n%16));	# f to hold Xupdate(xi,xa,xb,xc,xd)
	&rotr($b,2);			# b=ROTATE(b,30)
	 &xor($f,&swtmp(($n+2)%16));
	&xor($tmp1,$c);
	 &xor($f,&swtmp(($n+8)%16));
	&xor($tmp1,$d);			# tmp1 holds F_20_39(b,c,d)
	 &xor($f,&swtmp(($n+13)%16));	# f holds xa^xb^xc^xd
	&rotl($f,1);			# f=ROTATE(f,1)
	 &add($tmp1,$e);
	&mov(&swtmp($n%16),$f);		# xi=f
	 &mov($e,$a);			# e becomes volatile
	&rotl($e,5);			# e=ROTATE(a,5)
	 &lea($f,&DWP($K,$f,$tmp1));	# f+=K_20_39+e
	&add($f,$e);			# f+=ROTATE(a,5)
	}

sub BODY_40_59
	{
	local($n,$a,$b,$c,$d,$e,$f)=@_;

	&comment("40_59 $n");

	&mov($f,&swtmp($n%16));		# f to hold Xupdate(xi,xa,xb,xc,xd)
	 &mov($tmp1,&swtmp(($n+2)%16));
	&xor($f,$tmp1);
	 &mov($tmp1,&swtmp(($n+8)%16));
	&xor($f,$tmp1);
	 &mov($tmp1,&swtmp(($n+13)%16));
	&xor($f,$tmp1);			# f holds xa^xb^xc^xd
	 &mov($tmp1,$b);		# tmp1 to hold F_40_59(b,c,d)
	&rotl($f,1);			# f=ROTATE(f,1)
	 &or($tmp1,$c);
	&mov(&swtmp($n%16),$f);		# xi=f
	 &and($tmp1,$d);
	&lea($f,&DWP(0x8f1bbcdc,$f,$e));# f+=K_40_59+e
	 &mov($e,$b);			# e becomes volatile and is used
					# to calculate F_40_59(b,c,d)
	&rotr($b,2);			# b=ROTATE(b,30)
	 &and($e,$c);
	&or($tmp1,$e);			# tmp1 holds F_40_59(b,c,d)		
	 &mov($e,$a);
	&rotl($e,5);			# e=ROTATE(a,5)
	 &add($f,$tmp1);		# f+=tmp1;
	&add($f,$e);			# f+=ROTATE(a,5)
	}

&function_begin("sha1_block_data_order");
	&mov($tmp1,&wparam(0));	# SHA_CTX *c
	&mov($T,&wparam(1));	# const void *input
	&mov($A,&wparam(2));	# size_t num
	&stack_push(16);	# allocate X[16]
	&shl($A,6);
	&add($A,$T);
	&mov(&wparam(2),$A);	# pointer beyond the end of input
	&mov($E,&DWP(16,$tmp1));# pre-load E

	&set_label("loop",16);

	# copy input chunk to X, but reversing byte order!
	for ($i=0; $i<16; $i+=4)
		{
		&mov($A,&DWP(4*($i+0),$T));
		&mov($B,&DWP(4*($i+1),$T));
		&mov($C,&DWP(4*($i+2),$T));
		&mov($D,&DWP(4*($i+3),$T));
		&bswap($A);
		&bswap($B);
		&bswap($C);
		&bswap($D);
		&mov(&swtmp($i+0),$A);
		&mov(&swtmp($i+1),$B);
		&mov(&swtmp($i+2),$C);
		&mov(&swtmp($i+3),$D);
		}
	&mov(&wparam(1),$T);	# redundant in 1st spin

	&mov($A,&DWP(0,$tmp1));	# load SHA_CTX
	&mov($B,&DWP(4,$tmp1));
	&mov($C,&DWP(8,$tmp1));
	&mov($D,&DWP(12,$tmp1));
	# E is pre-loaded

	for($i=0;$i<16;$i++)	{ &BODY_00_15($i,@V); unshift(@V,pop(@V)); }
	for(;$i<20;$i++)	{ &BODY_16_19($i,@V); unshift(@V,pop(@V)); }
	for(;$i<40;$i++)	{ &BODY_20_39($i,@V); unshift(@V,pop(@V)); }
	for(;$i<60;$i++)	{ &BODY_40_59($i,@V); unshift(@V,pop(@V)); }
	for(;$i<80;$i++)	{ &BODY_20_39($i,@V); unshift(@V,pop(@V)); }

	(($V[5] eq $D) and ($V[0] eq $E)) or die;	# double-check

	&mov($tmp1,&wparam(0));	# re-load SHA_CTX*
	&mov($D,&wparam(1));	# D is last "T" and is discarded

	&add($E,&DWP(0,$tmp1));	# E is last "A"...
	&add($T,&DWP(4,$tmp1));
	&add($A,&DWP(8,$tmp1));
	&add($B,&DWP(12,$tmp1));
	&add($C,&DWP(16,$tmp1));

	&mov(&DWP(0,$tmp1),$E);	# update SHA_CTX
	 &add($D,64);		# advance input pointer
	&mov(&DWP(4,$tmp1),$T);
	 &cmp($D,&wparam(2));	# have we reached the end yet?
	&mov(&DWP(8,$tmp1),$A);
	 &mov($E,$C);		# C is last "E" which needs to be "pre-loaded"
	&mov(&DWP(12,$tmp1),$B);
	 &mov($T,$D);		# input pointer
	&mov(&DWP(16,$tmp1),$C);
	&jb(&label("loop"));

	&stack_pop(16);
&function_end("sha1_block_data_order");

&asm_finish();
