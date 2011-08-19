#!/usr/local/bin/perl
#
# The inner loop instruction sequence and the IP/FP modifications are from
# Svend Olaf Mikkelsen <svolaf@inet.uni-c.dk>
#

push(@INC,"perlasm","../../perlasm");
require "x86asm.pl";
require "cbc.pl";
require "desboth.pl";

# base code is in microsft
# op dest, source
# format.
#

&asm_init($ARGV[0],"des-586.pl");

$L="edi";
$R="esi";

&external_label("DES_SPtrans");
&DES_encrypt("DES_encrypt1",1);
&DES_encrypt("DES_encrypt2",0);
&DES_encrypt3("DES_encrypt3",1);
&DES_encrypt3("DES_decrypt3",0);
&cbc("DES_ncbc_encrypt","DES_encrypt1","DES_encrypt1",0,4,5,3,5,-1);
&cbc("DES_ede3_cbc_encrypt","DES_encrypt3","DES_decrypt3",0,6,7,3,4,5);

&asm_finish();

sub DES_encrypt
	{
	local($name,$do_ip)=@_;

	&function_begin_B($name,"EXTRN   _DES_SPtrans:DWORD");

	&push("esi");
	&push("edi");

	&comment("");
	&comment("Load the 2 words");
	$trans="ebp";

	if ($do_ip)
		{
		&mov($R,&wparam(0));
		 &xor(	"ecx",		"ecx"		);

		&push("ebx");
		&push("ebp");

		&mov("eax",&DWP(0,$R,"",0));
		 &mov("ebx",&wparam(2));	# get encrypt flag
		&mov($L,&DWP(4,$R,"",0));
		&comment("");
		&comment("IP");
		&IP_new("eax",$L,$R,3);
		}
	else
		{
		&mov("eax",&wparam(0));
		 &xor(	"ecx",		"ecx"		);

		&push("ebx");
		&push("ebp");

		&mov($R,&DWP(0,"eax","",0));
		 &mov("ebx",&wparam(2));	# get encrypt flag
		&rotl($R,3);
		&mov($L,&DWP(4,"eax","",0));
		&rotl($L,3);
		}

	# PIC-ification:-)
	&picmeup($trans,"DES_SPtrans");
	#if ($cpp)	{ &picmeup($trans,"DES_SPtrans");   }
	#else		{ &lea($trans,&DWP("DES_SPtrans")); }

	&mov(	"ecx",	&wparam(1)	);
	&cmp("ebx","0");
	&je(&label("start_decrypt"));

	for ($i=0; $i<16; $i+=2)
		{
		&comment("");
		&comment("Round $i");
		&D_ENCRYPT($i,$L,$R,$i*2,$trans,"eax","ebx","ecx","edx");

		&comment("");
		&comment("Round ".sprintf("%d",$i+1));
		&D_ENCRYPT($i+1,$R,$L,($i+1)*2,$trans,"eax","ebx","ecx","edx");
		}
	&jmp(&label("end"));

	&set_label("start_decrypt");

	for ($i=15; $i>0; $i-=2)
		{
		&comment("");
		&comment("Round $i");
		&D_ENCRYPT(15-$i,$L,$R,$i*2,$trans,"eax","ebx","ecx","edx");
		&comment("");
		&comment("Round ".sprintf("%d",$i-1));
		&D_ENCRYPT(15-$i+1,$R,$L,($i-1)*2,$trans,"eax","ebx","ecx","edx");
		}

	&set_label("end");

	if ($do_ip)
		{
		&comment("");
		&comment("FP");
		&mov("edx",&wparam(0));
		&FP_new($L,$R,"eax",3);

		&mov(&DWP(0,"edx","",0),"eax");
		&mov(&DWP(4,"edx","",0),$R);
		}
	else
		{
		&comment("");
		&comment("Fixup");
		&rotr($L,3);		# r
		 &mov("eax",&wparam(0));
		&rotr($R,3);		# l
		 &mov(&DWP(0,"eax","",0),$L);
		 &mov(&DWP(4,"eax","",0),$R);
		}

	&pop("ebp");
	&pop("ebx");
	&pop("edi");
	&pop("esi");
	&ret();

	&function_end_B($name);
	}

sub D_ENCRYPT
	{
	local($r,$L,$R,$S,$trans,$u,$tmp1,$tmp2,$t)=@_;

	 &mov(	$u,		&DWP(&n2a($S*4),$tmp2,"",0));
	&xor(	$tmp1,		$tmp1);
	 &mov(	$t,		&DWP(&n2a(($S+1)*4),$tmp2,"",0));
	&xor(	$u,		$R);
	&xor(	$tmp2,		$tmp2);
	 &xor(	$t,		$R);
	&and(	$u,		"0xfcfcfcfc"	);
	 &and(	$t,		"0xcfcfcfcf"	);
	&movb(	&LB($tmp1),	&LB($u)	);
	 &movb(	&LB($tmp2),	&HB($u)	);
	&rotr(	$t,		4		);
	&xor(	$L,		&DWP("     ",$trans,$tmp1,0));
	 &movb(	&LB($tmp1),	&LB($t)	);
	 &xor(	$L,		&DWP("0x200",$trans,$tmp2,0));
	 &movb(	&LB($tmp2),	&HB($t)	);
	&shr(	$u,		16);
	 &xor(	$L,		&DWP("0x100",$trans,$tmp1,0));
	 &movb(	&LB($tmp1),	&HB($u)	);
	&shr(	$t,		16);
	 &xor(	$L,		&DWP("0x300",$trans,$tmp2,0));
	&movb(	&LB($tmp2),	&HB($t)	);
	 &and(	$u,		"0xff"	);
	&and(	$t,		"0xff"	);
	 &xor(	$L,		&DWP("0x600",$trans,$tmp1,0));
	 &xor(	$L,		&DWP("0x700",$trans,$tmp2,0));
	&mov(	$tmp2,		&wparam(1)	);
	 &xor(	$L,		&DWP("0x400",$trans,$u,0));
	 &xor(	$L,		&DWP("0x500",$trans,$t,0));
	}

sub n2a
	{
	sprintf("%d",$_[0]);
	}

# now has a side affect of rotating $a by $shift
sub R_PERM_OP
	{
	local($a,$b,$tt,$shift,$mask,$last)=@_;

	&rotl(	$a,		$shift		) if ($shift != 0);
	&mov(	$tt,		$a		);
	&xor(	$a,		$b		);
	&and(	$a,		$mask		);
	# This can never succeed, and besides it is difficult to see what the
	# idea was - Ben 13 Feb 99
	if (!$last eq $b)
		{
		&xor(	$b,		$a		);
		&xor(	$tt,		$a		);
		}
	else
		{
		&xor(	$tt,		$a		);
		&xor(	$b,		$a		);
		}
	&comment("");
	}

sub IP_new
	{
	local($l,$r,$tt,$lr)=@_;

	&R_PERM_OP($l,$r,$tt, 4,"0xf0f0f0f0",$l);
	&R_PERM_OP($r,$tt,$l,20,"0xfff0000f",$l);
	&R_PERM_OP($l,$tt,$r,14,"0x33333333",$r);
	&R_PERM_OP($tt,$r,$l,22,"0x03fc03fc",$r);
	&R_PERM_OP($l,$r,$tt, 9,"0xaaaaaaaa",$r);
	
	if ($lr != 3)
		{
		if (($lr-3) < 0)
			{ &rotr($tt,	3-$lr); }
		else	{ &rotl($tt,	$lr-3); }
		}
	if ($lr != 2)
		{
		if (($lr-2) < 0)
			{ &rotr($r,	2-$lr); }
		else	{ &rotl($r,	$lr-2); }
		}
	}

sub FP_new
	{
	local($l,$r,$tt,$lr)=@_;

	if ($lr != 2)
		{
		if (($lr-2) < 0)
			{ &rotl($r,	2-$lr); }
		else	{ &rotr($r,	$lr-2); }
		}
	if ($lr != 3)
		{
		if (($lr-3) < 0)
			{ &rotl($l,	3-$lr); }
		else	{ &rotr($l,	$lr-3); }
		}

	&R_PERM_OP($l,$r,$tt, 0,"0xaaaaaaaa",$r);
	&R_PERM_OP($tt,$r,$l,23,"0x03fc03fc",$r);
	&R_PERM_OP($l,$r,$tt,10,"0x33333333",$l);
	&R_PERM_OP($r,$tt,$l,18,"0xfff0000f",$l);
	&R_PERM_OP($l,$tt,$r,12,"0xf0f0f0f0",$r);
	&rotr($tt	, 4);
	}

