#!/usr/local/bin/perl
# x86 assember

sub bn_mul_words
	{
	local($name)=@_;

	&function_begin($name,"");

	&comment("");
	$Low="eax";
	$High="edx";
	$a="ebx";
	$w="ecx";
	$r="edi";
	$c="esi";
	$num="ebp";

	&xor($c,$c);		# clear carry
	&mov($r,&wparam(0));	#
	&mov($a,&wparam(1));	#
	&mov($num,&wparam(2));	#
	&mov($w,&wparam(3));	#

	&and($num,0xfffffff8);	# num / 8
	&jz(&label("mw_finish"));

	&set_label("mw_loop",0);
	for ($i=0; $i<32; $i+=4)
		{
		&comment("Round $i");

		 &mov("eax",&DWP($i,$a,"",0)); 	# *a
		&mul($w);			# *a * w
		&add("eax",$c);			# L(t)+=c
		 # XXX

		&adc("edx",0);			# H(t)+=carry
		 &mov(&DWP($i,$r,"",0),"eax");	# *r= L(t);

		&mov($c,"edx");			# c=  H(t);
		}

	&comment("");
	&add($a,32);
	&add($r,32);
	&sub($num,8);
	&jz(&label("mw_finish"));
	&jmp(&label("mw_loop"));

	&set_label("mw_finish",0);
	&mov($num,&wparam(2));	# get num
	&and($num,7);
	&jnz(&label("mw_finish2"));
	&jmp(&label("mw_end"));

	&set_label("mw_finish2",1);
	for ($i=0; $i<7; $i++)
		{
		&comment("Tail Round $i");
		 &mov("eax",&DWP($i*4,$a,"",0));# *a
		&mul($w);			# *a * w
		&add("eax",$c);			# L(t)+=c
		 # XXX
		&adc("edx",0);			# H(t)+=carry
		 &mov(&DWP($i*4,$r,"",0),"eax");# *r= L(t);
		&mov($c,"edx");			# c=  H(t);
		 &dec($num) if ($i != 7-1);
		&jz(&label("mw_end")) if ($i != 7-1);
		}
	&set_label("mw_end",0);
	&mov("eax",$c);

	&function_end($name);
	}

1;
