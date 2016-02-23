#!/usr/local/bin/perl

push(@INC,"perlasm","../../perlasm");
require "x86asm.pl";
require "cbc.pl";

&asm_init($ARGV[0],"bf-686.pl");

$BF_ROUNDS=16;
$BF_OFF=($BF_ROUNDS+2)*4;
$L="ecx";
$R="edx";
$P="edi";
$tot="esi";
$tmp1="eax";
$tmp2="ebx";
$tmp3="ebp";

&des_encrypt("BF_encrypt",1);
&des_encrypt("BF_decrypt",0);
&cbc("BF_cbc_encrypt","BF_encrypt","BF_decrypt",1,4,5,3,-1,-1);

&asm_finish();

&file_end();

sub des_encrypt
	{
	local($name,$enc)=@_;

	&function_begin($name,"");

	&comment("");
	&comment("Load the 2 words");
	&mov("eax",&wparam(0));
	&mov($L,&DWP(0,"eax","",0));
	&mov($R,&DWP(4,"eax","",0));

	&comment("");
	&comment("P pointer, s and enc flag");
	&mov($P,&wparam(1));

	&xor(	$tmp1,	$tmp1);
	&xor(	$tmp2,	$tmp2);

	# encrypting part

	if ($enc)
		{
		&xor($L,&DWP(0,$P,"",0));
		for ($i=0; $i<$BF_ROUNDS; $i+=2)
			{
			&comment("");
			&comment("Round $i");
			&BF_ENCRYPT($i+1,$R,$L,$P,$tot,$tmp1,$tmp2,$tmp3);

			&comment("");
			&comment("Round ".sprintf("%d",$i+1));
			&BF_ENCRYPT($i+2,$L,$R,$P,$tot,$tmp1,$tmp2,$tmp3);
			}
		&xor($R,&DWP(($BF_ROUNDS+1)*4,$P,"",0));

		&mov("eax",&wparam(0));
		&mov(&DWP(0,"eax","",0),$R);
		&mov(&DWP(4,"eax","",0),$L);
		&function_end_A($name);
		}
	else
		{
		&xor($L,&DWP(($BF_ROUNDS+1)*4,$P,"",0));
		for ($i=$BF_ROUNDS; $i>0; $i-=2)
			{
			&comment("");
			&comment("Round $i");
			&BF_ENCRYPT($i,$R,$L,$P,$tot,$tmp1,$tmp2,$tmp3);
			&comment("");
			&comment("Round ".sprintf("%d",$i-1));
			&BF_ENCRYPT($i-1,$L,$R,$P,$tot,$tmp1,$tmp2,$tmp3);
			}
		&xor($R,&DWP(0,$P,"",0));

		&mov("eax",&wparam(0));
		&mov(&DWP(0,"eax","",0),$R);
		&mov(&DWP(4,"eax","",0),$L);
		&function_end_A($name);
		}

	&function_end_B($name);
	}

sub BF_ENCRYPT
	{
	local($i,$L,$R,$P,$tot,$tmp1,$tmp2,$tmp3)=@_;

	&rotr(	$R,		16);
	&mov(	$tot,		&DWP(&n2a($i*4),$P,"",0));

	&movb(	&LB($tmp1),	&HB($R));
	&movb(	&LB($tmp2),	&LB($R));

	&rotr(	$R,		16);
	&xor(	$L,		$tot);

	&mov(	$tot,		&DWP(&n2a($BF_OFF+0x0000),$P,$tmp1,4));
	&mov(	$tmp3,		&DWP(&n2a($BF_OFF+0x0400),$P,$tmp2,4));

	&movb(	&LB($tmp1),	&HB($R));
	&movb(	&LB($tmp2),	&LB($R));

	&add(	$tot,		$tmp3);
	&mov(	$tmp1,		&DWP(&n2a($BF_OFF+0x0800),$P,$tmp1,4)); # delay

	&xor(	$tot,		$tmp1);
	&mov(	$tmp3,		&DWP(&n2a($BF_OFF+0x0C00),$P,$tmp2,4));

	&add(	$tot,		$tmp3);
	&xor(	$tmp1,		$tmp1);

	&xor(	$L,		$tot);					
	# delay
	}

sub n2a
	{
	sprintf("%d",$_[0]);
	}

