#!/usr/local/bin/perl

push(@INC,"perlasm","../../perlasm");
require "x86asm.pl";

require("x86/mul_add.pl");
require("x86/mul.pl");
require("x86/sqr.pl");
require("x86/div.pl");
require("x86/add.pl");
require("x86/sub.pl");
require("x86/comba.pl");

&asm_init($ARGV[0],$0);

&bn_mul_add_words("bn_mul_add_words");
&bn_mul_words("bn_mul_words");
&bn_sqr_words("bn_sqr_words");
&bn_div_words("bn_div_words");
&bn_add_words("bn_add_words");
&bn_sub_words("bn_sub_words");
&bn_mul_comba("bn_mul_comba8",8);
&bn_mul_comba("bn_mul_comba4",4);
&bn_sqr_comba("bn_sqr_comba8",8);
&bn_sqr_comba("bn_sqr_comba4",4);

&asm_finish();

