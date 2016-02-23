#!/usr/local/bin/perl
# x86 assember

sub bn_div_words
	{
	local($name)=@_;

	&function_begin($name,"");
	&mov("edx",&wparam(0));	#
	&mov("eax",&wparam(1));	#
	&mov("ebx",&wparam(2));	#
	&div("ebx");
	&function_end($name);
	}
1;
