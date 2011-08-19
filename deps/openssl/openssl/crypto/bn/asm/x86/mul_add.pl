#!/usr/local/bin/perl
# x86 assember

sub bn_mul_add_words
	{
	local($name)=@_;

	&function_begin($name,"");

	&comment("");
	$Low="eax";
	$High="edx";
	$a="ebx";
	$w="ebp";
	$r="edi";
	$c="esi";

	&xor($c,$c);		# clear carry
	&mov($r,&wparam(0));	#

	&mov("ecx",&wparam(2));	#
	&mov($a,&wparam(1));	#

	&and("ecx",0xfffffff8);	# num / 8
	&mov($w,&wparam(3));	#

	&push("ecx");		# Up the stack for a tmp variable

	&jz(&label("maw_finish"));

	&set_label("maw_loop",0);

	&mov(&swtmp(0),"ecx");	#

	for ($i=0; $i<32; $i+=4)
		{
		&comment("Round $i");

		 &mov("eax",&DWP($i,$a,"",0)); 	# *a
		&mul($w);			# *a * w
		&add("eax",$c);		# L(t)+= *r
		 &mov($c,&DWP($i,$r,"",0));	# L(t)+= *r
		&adc("edx",0);			# H(t)+=carry
		 &add("eax",$c);		# L(t)+=c
		&adc("edx",0);			# H(t)+=carry
		 &mov(&DWP($i,$r,"",0),"eax");	# *r= L(t);
		&mov($c,"edx");			# c=  H(t);
		}

	&comment("");
	&mov("ecx",&swtmp(0));	#
	&add($a,32);
	&add($r,32);
	&sub("ecx",8);
	&jnz(&label("maw_loop"));

	&set_label("maw_finish",0);
	&mov("ecx",&wparam(2));	# get num
	&and("ecx",7);
	&jnz(&label("maw_finish2"));	# helps branch prediction
	&jmp(&label("maw_end"));

	&set_label("maw_finish2",1);
	for ($i=0; $i<7; $i++)
		{
		&comment("Tail Round $i");
		 &mov("eax",&DWP($i*4,$a,"",0));# *a
		&mul($w);			# *a * w
		&add("eax",$c);			# L(t)+=c
		 &mov($c,&DWP($i*4,$r,"",0));	# L(t)+= *r
		&adc("edx",0);			# H(t)+=carry
		 &add("eax",$c);
		&adc("edx",0);			# H(t)+=carry
		 &dec("ecx") if ($i != 7-1);
		&mov(&DWP($i*4,$r,"",0),"eax");	# *r= L(t);
		 &mov($c,"edx");			# c=  H(t);
		&jz(&label("maw_end")) if ($i != 7-1);
		}
	&set_label("maw_end",0);
	&mov("eax",$c);

	&pop("ecx");	# clear variable from

	&function_end($name);
	}

1;
