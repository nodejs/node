#!/usr/bin/env perl
#
# ====================================================================
# Written by Andy Polyakov <appro@fy.chalmers.se> for the OpenSSL
# project. Rights for redistribution and usage in source and binary
# forms are granted according to the OpenSSL license.
# ====================================================================
#
# SHA512_Transform_SSE2.
#
# As the name suggests, this is an IA-32 SSE2 implementation of
# SHA512_Transform. Motivating factor for the undertaken effort was that
# SHA512 was observed to *consistently* perform *significantly* poorer
# than SHA256 [2x and slower is common] on 32-bit platforms. On 64-bit
# platforms on the other hand SHA512 tend to outperform SHA256 [~50%
# seem to be common improvement factor]. All this is perfectly natural,
# as SHA512 is a 64-bit algorithm. But isn't IA-32 SSE2 essentially
# a 64-bit instruction set? Is it rich enough to implement SHA512?
# If answer was "no," then you wouldn't have been reading this...
#
# Throughput performance in MBps (larger is better):
#
#		2.4GHz P4	1.4GHz AMD32	1.4GHz AMD64(*)
# SHA256/gcc(*)	54		43		59
# SHA512/gcc	17		23		92
# SHA512/sse2	61(**)		57(**)
# SHA512/icc	26		28
# SHA256/icc(*)	65		54
#
# (*)	AMD64 and SHA256 numbers are presented mostly for amusement or
#	reference purposes.
# (**)	I.e. it gives ~2-3x speed-up if compared with compiler generated
#	code. One can argue that hand-coded *non*-SSE2 implementation
#	would perform better than compiler generated one as well, and
#	that comparison is therefore not exactly fair. Well, as SHA512
#	puts enormous pressure on IA-32 GP register bank, I reckon that
#	hand-coded version wouldn't perform significantly better than
#	one compiled with icc, ~20% perhaps... So that this code would
#	still outperform it with distinguishing marginal. But feel free
#	to prove me wrong:-)
#						<appro@fy.chalmers.se>
push(@INC,"perlasm","../../perlasm");
require "x86asm.pl";

&asm_init($ARGV[0],"sha512-sse2.pl",$ARGV[$#ARGV] eq "386");

$K512="esi";	# K512[80] table, found at the end...
#$W512="esp";	# $W512 is not just W512[16]: it comprises *two* copies
		# of W512[16] and a copy of A-H variables...
$W512_SZ=8*(16+16+8);	# see above...
#$Kidx="ebx";	# index in K512 table, advances from 0 to 80...
$Widx="edx";	# index in W512, wraps around at 16...
$data="edi";	# 16 qwords of input data...
$A="mm0";	# B-D and
$E="mm1";	# F-H are allocated dynamically...
$Aoff=256+0;	# A-H offsets relative to $W512...
$Boff=256+8;
$Coff=256+16;
$Doff=256+24;
$Eoff=256+32;
$Foff=256+40;
$Goff=256+48;
$Hoff=256+56;

sub SHA2_ROUND()
{ local ($kidx,$widx)=@_;

	# One can argue that one could reorder instructions for better
	# performance. Well, I tried and it doesn't seem to make any
	# noticeable difference. Modern out-of-order execution cores
	# reorder instructions to their liking in either case and they
	# apparently do decent job. So we can keep the code more
	# readable/regular/comprehensible:-)

	# I adhere to 64-bit %mmX registers in order to avoid/not care
	# about #GP exceptions on misaligned 128-bit access, most
	# notably in paddq with memory operand. Not to mention that
	# SSE2 intructions operating on %mmX can be scheduled every
	# cycle [and not every second one if operating on %xmmN].

	&movq	("mm4",&QWP($Foff,$W512));	# load f
	&movq	("mm5",&QWP($Goff,$W512));	# load g
	&movq	("mm6",&QWP($Hoff,$W512));	# load h

	&movq	("mm2",$E);			# %mm2 is sliding right
	&movq	("mm3",$E);			# %mm3 is sliding left
	&psrlq	("mm2",14);
	&psllq	("mm3",23);
	&movq	("mm7","mm2");			# %mm7 is T1
	&pxor	("mm7","mm3");
	&psrlq	("mm2",4);
	&psllq	("mm3",23);
	&pxor	("mm7","mm2");
	&pxor	("mm7","mm3");
	&psrlq	("mm2",23);
	&psllq	("mm3",4);
	&pxor	("mm7","mm2");
	&pxor	("mm7","mm3");			# T1=Sigma1_512(e)

	&movq	(&QWP($Foff,$W512),$E);		# f = e
	&movq	(&QWP($Goff,$W512),"mm4");	# g = f
	&movq	(&QWP($Hoff,$W512),"mm5");	# h = g

	&pxor	("mm4","mm5");			# f^=g
	&pand	("mm4",$E);			# f&=e
	&pxor	("mm4","mm5");			# f^=g
	&paddq	("mm7","mm4");			# T1+=Ch(e,f,g)

	&movq	("mm2",&QWP($Boff,$W512));	# load b
	&movq	("mm3",&QWP($Coff,$W512));	# load c
	&movq	($E,&QWP($Doff,$W512));		# e = d

	&paddq	("mm7","mm6");			# T1+=h
	&paddq	("mm7",&QWP(0,$K512,$kidx,8));	# T1+=K512[i]
	&paddq	("mm7",&QWP(0,$W512,$widx,8));	# T1+=W512[i]
	&paddq	($E,"mm7");			# e += T1

	&movq	("mm4",$A);			# %mm4 is sliding right
	&movq	("mm5",$A);			# %mm5 is sliding left
	&psrlq	("mm4",28);
	&psllq	("mm5",25);
	&movq	("mm6","mm4");			# %mm6 is T2
	&pxor	("mm6","mm5");
	&psrlq	("mm4",6);
	&psllq	("mm5",5);
	&pxor	("mm6","mm4");
	&pxor	("mm6","mm5");
	&psrlq	("mm4",5);
	&psllq	("mm5",6);
	&pxor	("mm6","mm4");
	&pxor	("mm6","mm5");			# T2=Sigma0_512(a)

	&movq	(&QWP($Boff,$W512),$A);		# b = a
	&movq	(&QWP($Coff,$W512),"mm2");	# c = b
	&movq	(&QWP($Doff,$W512),"mm3");	# d = c

	&movq	("mm4",$A);			# %mm4=a
	&por	($A,"mm3");			# a=a|c
	&pand	("mm4","mm3");			# %mm4=a&c
	&pand	($A,"mm2");			# a=(a|c)&b
	&por	("mm4",$A);			# %mm4=(a&c)|((a|c)&b)
	&paddq	("mm6","mm4");			# T2+=Maj(a,b,c)

	&movq	($A,"mm7");			# a=T1
	&paddq	($A,"mm6");			# a+=T2
}

$func="sha512_block_sse2";

&function_begin_B($func);
	if (0) {# Caller is expected to check if it's appropriate to
		# call this routine. Below 3 lines are retained for
		# debugging purposes...
		&picmeup("eax","OPENSSL_ia32cap");
		&bt	(&DWP(0,"eax"),26);
		&jnc	("SHA512_Transform");
	}

	&push	("ebp");
	&mov	("ebp","esp");
	&push	("ebx");
	&push	("esi");
	&push	("edi");

	&mov	($Widx,&DWP(8,"ebp"));		# A-H state, 1st arg
	&mov	($data,&DWP(12,"ebp"));		# input data, 2nd arg
	&call	(&label("pic_point"));		# make it PIC!
&set_label("pic_point");
	&blindpop($K512);
	&lea	($K512,&DWP(&label("K512")."-".&label("pic_point"),$K512));

	$W512 = "esp";			# start using %esp as W512
	&sub	($W512,$W512_SZ);
	&and	($W512,-16);		# ensure 128-bit alignment

	# make private copy of A-H
	#     v assume the worst and stick to unaligned load
	&movdqu	("xmm0",&QWP(0,$Widx));
	&movdqu	("xmm1",&QWP(16,$Widx));
	&movdqu	("xmm2",&QWP(32,$Widx));
	&movdqu	("xmm3",&QWP(48,$Widx));

&align(8);
&set_label("_chunk_loop");

	&movdqa	(&QWP($Aoff,$W512),"xmm0");	# a,b
	&movdqa	(&QWP($Coff,$W512),"xmm1");	# c,d
	&movdqa	(&QWP($Eoff,$W512),"xmm2");	# e,f
	&movdqa	(&QWP($Goff,$W512),"xmm3");	# g,h

	&xor	($Widx,$Widx);

	&movdq2q($A,"xmm0");			# load a
	&movdq2q($E,"xmm2");			# load e

	# Why aren't loops unrolled? It makes sense to unroll if
	# execution time for loop body is comparable with branch
	# penalties and/or if whole data-set resides in register bank.
	# Neither is case here... Well, it would be possible to
	# eliminate few store operations, but it would hardly affect
	# so to say stop-watch performance, as there is a lot of
	# available memory slots to fill. It will only relieve some
	# pressure off memory bus...

	# flip input stream byte order...
	&mov	("eax",&DWP(0,$data,$Widx,8));
	&mov	("ebx",&DWP(4,$data,$Widx,8));
	&bswap	("eax");
	&bswap	("ebx");
	&mov	(&DWP(0,$W512,$Widx,8),"ebx");		# W512[i]
	&mov	(&DWP(4,$W512,$Widx,8),"eax");
	&mov	(&DWP(128+0,$W512,$Widx,8),"ebx");	# copy of W512[i]
	&mov	(&DWP(128+4,$W512,$Widx,8),"eax");

&align(8);
&set_label("_1st_loop");		# 0-15
	# flip input stream byte order...
	&mov	("eax",&DWP(0+8,$data,$Widx,8));
	&mov	("ebx",&DWP(4+8,$data,$Widx,8));
	&bswap	("eax");
	&bswap	("ebx");
	&mov	(&DWP(0+8,$W512,$Widx,8),"ebx");	# W512[i]
	&mov	(&DWP(4+8,$W512,$Widx,8),"eax");
	&mov	(&DWP(128+0+8,$W512,$Widx,8),"ebx");	# copy of W512[i]
	&mov	(&DWP(128+4+8,$W512,$Widx,8),"eax");
&set_label("_1st_looplet");
	&SHA2_ROUND($Widx,$Widx); &inc($Widx);

&cmp	($Widx,15)
&jl	(&label("_1st_loop"));
&je	(&label("_1st_looplet"));	# playing similar trick on 2nd loop
					# does not improve performance...

	$Kidx = "ebx";			# start using %ebx as Kidx
	&mov	($Kidx,$Widx);

&align(8);
&set_label("_2nd_loop");		# 16-79
	&and($Widx,0xf);

	# 128-bit fragment! I update W512[i] and W512[i+1] in
	# parallel:-) Note that I refer to W512[(i&0xf)+N] and not to
	# W512[(i+N)&0xf]! This is exactly what I maintain the second
	# copy of W512[16] for...
	&movdqu	("xmm0",&QWP(8*1,$W512,$Widx,8));	# s0=W512[i+1]
	&movdqa	("xmm2","xmm0");		# %xmm2 is sliding right
	&movdqa	("xmm3","xmm0");		# %xmm3 is sliding left
	&psrlq	("xmm2",1);
	&psllq	("xmm3",56);
	&movdqa	("xmm0","xmm2");
	&pxor	("xmm0","xmm3");
	&psrlq	("xmm2",6);
	&psllq	("xmm3",7);
	&pxor	("xmm0","xmm2");
	&pxor	("xmm0","xmm3");
	&psrlq	("xmm2",1);
	&pxor	("xmm0","xmm2");		# s0 = sigma0_512(s0);

	&movdqa	("xmm1",&QWP(8*14,$W512,$Widx,8));	# s1=W512[i+14]
	&movdqa	("xmm4","xmm1");		# %xmm4 is sliding right
	&movdqa	("xmm5","xmm1");		# %xmm5 is sliding left
	&psrlq	("xmm4",6);
	&psllq	("xmm5",3);
	&movdqa	("xmm1","xmm4");
	&pxor	("xmm1","xmm5");
	&psrlq	("xmm4",13);
	&psllq	("xmm5",42);
	&pxor	("xmm1","xmm4");
	&pxor	("xmm1","xmm5");
	&psrlq	("xmm4",42);
	&pxor	("xmm1","xmm4");		# s1 = sigma1_512(s1);

	#     + have to explictly load W512[i+9] as it's not 128-bit
	#     v	aligned and paddq would throw an exception...
	&movdqu	("xmm6",&QWP(8*9,$W512,$Widx,8));
	&paddq	("xmm0","xmm1");		# s0 += s1
	&paddq	("xmm0","xmm6");		# s0 += W512[i+9]
	&paddq	("xmm0",&QWP(0,$W512,$Widx,8));	# s0 += W512[i]

	&movdqa	(&QWP(0,$W512,$Widx,8),"xmm0");		# W512[i] = s0
	&movdqa	(&QWP(16*8,$W512,$Widx,8),"xmm0");	# copy of W512[i]

	# as the above fragment was 128-bit, we "owe" 2 rounds...
	&SHA2_ROUND($Kidx,$Widx); &inc($Kidx); &inc($Widx);
	&SHA2_ROUND($Kidx,$Widx); &inc($Kidx); &inc($Widx);

&cmp	($Kidx,80);
&jl	(&label("_2nd_loop"));

	# update A-H state
	&mov	($Widx,&DWP(8,"ebp"));		# A-H state, 1st arg
	&movq	(&QWP($Aoff,$W512),$A);		# write out a
	&movq	(&QWP($Eoff,$W512),$E);		# write out e
	&movdqu	("xmm0",&QWP(0,$Widx));
	&movdqu	("xmm1",&QWP(16,$Widx));
	&movdqu	("xmm2",&QWP(32,$Widx));
	&movdqu	("xmm3",&QWP(48,$Widx));
	&paddq	("xmm0",&QWP($Aoff,$W512));	# 128-bit additions...
	&paddq	("xmm1",&QWP($Coff,$W512));
	&paddq	("xmm2",&QWP($Eoff,$W512));
	&paddq	("xmm3",&QWP($Goff,$W512));
	&movdqu	(&QWP(0,$Widx),"xmm0");
	&movdqu	(&QWP(16,$Widx),"xmm1");
	&movdqu	(&QWP(32,$Widx),"xmm2");
	&movdqu	(&QWP(48,$Widx),"xmm3");

&add	($data,16*8);				# advance input data pointer
&dec	(&DWP(16,"ebp"));			# decrement 3rd arg
&jnz	(&label("_chunk_loop"));

	# epilogue
	&emms	();	# required for at least ELF and Win32 ABIs
	&mov	("edi",&DWP(-12,"ebp"));
	&mov	("esi",&DWP(-8,"ebp"));
	&mov	("ebx",&DWP(-4,"ebp"));
	&leave	();
&ret	();

&align(64);
&set_label("K512");	# Yes! I keep it in the code segment!
	&data_word(0xd728ae22,0x428a2f98);	# u64
	&data_word(0x23ef65cd,0x71374491);	# u64
	&data_word(0xec4d3b2f,0xb5c0fbcf);	# u64
	&data_word(0x8189dbbc,0xe9b5dba5);	# u64
	&data_word(0xf348b538,0x3956c25b);	# u64
	&data_word(0xb605d019,0x59f111f1);	# u64
	&data_word(0xaf194f9b,0x923f82a4);	# u64
	&data_word(0xda6d8118,0xab1c5ed5);	# u64
	&data_word(0xa3030242,0xd807aa98);	# u64
	&data_word(0x45706fbe,0x12835b01);	# u64
	&data_word(0x4ee4b28c,0x243185be);	# u64
	&data_word(0xd5ffb4e2,0x550c7dc3);	# u64
	&data_word(0xf27b896f,0x72be5d74);	# u64
	&data_word(0x3b1696b1,0x80deb1fe);	# u64
	&data_word(0x25c71235,0x9bdc06a7);	# u64
	&data_word(0xcf692694,0xc19bf174);	# u64
	&data_word(0x9ef14ad2,0xe49b69c1);	# u64
	&data_word(0x384f25e3,0xefbe4786);	# u64
	&data_word(0x8b8cd5b5,0x0fc19dc6);	# u64
	&data_word(0x77ac9c65,0x240ca1cc);	# u64
	&data_word(0x592b0275,0x2de92c6f);	# u64
	&data_word(0x6ea6e483,0x4a7484aa);	# u64
	&data_word(0xbd41fbd4,0x5cb0a9dc);	# u64
	&data_word(0x831153b5,0x76f988da);	# u64
	&data_word(0xee66dfab,0x983e5152);	# u64
	&data_word(0x2db43210,0xa831c66d);	# u64
	&data_word(0x98fb213f,0xb00327c8);	# u64
	&data_word(0xbeef0ee4,0xbf597fc7);	# u64
	&data_word(0x3da88fc2,0xc6e00bf3);	# u64
	&data_word(0x930aa725,0xd5a79147);	# u64
	&data_word(0xe003826f,0x06ca6351);	# u64
	&data_word(0x0a0e6e70,0x14292967);	# u64
	&data_word(0x46d22ffc,0x27b70a85);	# u64
	&data_word(0x5c26c926,0x2e1b2138);	# u64
	&data_word(0x5ac42aed,0x4d2c6dfc);	# u64
	&data_word(0x9d95b3df,0x53380d13);	# u64
	&data_word(0x8baf63de,0x650a7354);	# u64
	&data_word(0x3c77b2a8,0x766a0abb);	# u64
	&data_word(0x47edaee6,0x81c2c92e);	# u64
	&data_word(0x1482353b,0x92722c85);	# u64
	&data_word(0x4cf10364,0xa2bfe8a1);	# u64
	&data_word(0xbc423001,0xa81a664b);	# u64
	&data_word(0xd0f89791,0xc24b8b70);	# u64
	&data_word(0x0654be30,0xc76c51a3);	# u64
	&data_word(0xd6ef5218,0xd192e819);	# u64
	&data_word(0x5565a910,0xd6990624);	# u64
	&data_word(0x5771202a,0xf40e3585);	# u64
	&data_word(0x32bbd1b8,0x106aa070);	# u64
	&data_word(0xb8d2d0c8,0x19a4c116);	# u64
	&data_word(0x5141ab53,0x1e376c08);	# u64
	&data_word(0xdf8eeb99,0x2748774c);	# u64
	&data_word(0xe19b48a8,0x34b0bcb5);	# u64
	&data_word(0xc5c95a63,0x391c0cb3);	# u64
	&data_word(0xe3418acb,0x4ed8aa4a);	# u64
	&data_word(0x7763e373,0x5b9cca4f);	# u64
	&data_word(0xd6b2b8a3,0x682e6ff3);	# u64
	&data_word(0x5defb2fc,0x748f82ee);	# u64
	&data_word(0x43172f60,0x78a5636f);	# u64
	&data_word(0xa1f0ab72,0x84c87814);	# u64
	&data_word(0x1a6439ec,0x8cc70208);	# u64
	&data_word(0x23631e28,0x90befffa);	# u64
	&data_word(0xde82bde9,0xa4506ceb);	# u64
	&data_word(0xb2c67915,0xbef9a3f7);	# u64
	&data_word(0xe372532b,0xc67178f2);	# u64
	&data_word(0xea26619c,0xca273ece);	# u64
	&data_word(0x21c0c207,0xd186b8c7);	# u64
	&data_word(0xcde0eb1e,0xeada7dd6);	# u64
	&data_word(0xee6ed178,0xf57d4f7f);	# u64
	&data_word(0x72176fba,0x06f067aa);	# u64
	&data_word(0xa2c898a6,0x0a637dc5);	# u64
	&data_word(0xbef90dae,0x113f9804);	# u64
	&data_word(0x131c471b,0x1b710b35);	# u64
	&data_word(0x23047d84,0x28db77f5);	# u64
	&data_word(0x40c72493,0x32caab7b);	# u64
	&data_word(0x15c9bebc,0x3c9ebe0a);	# u64
	&data_word(0x9c100d4c,0x431d67c4);	# u64
	&data_word(0xcb3e42b6,0x4cc5d4be);	# u64
	&data_word(0xfc657e2a,0x597f299c);	# u64
	&data_word(0x3ad6faec,0x5fcb6fab);	# u64
	&data_word(0x4a475817,0x6c44198c);	# u64

&function_end_B($func);

&asm_finish();
