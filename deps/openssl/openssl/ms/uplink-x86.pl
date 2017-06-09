#!/usr/bin/env perl

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
push(@INC, "${dir}.", "${dir}../crypto/perlasm");
require "x86asm.pl";

require "uplink-common.pl";

&asm_init($ARGV[0],"uplink-x86");

&external_label("OPENSSL_Uplink");
&public_label("OPENSSL_UplinkTable");

for ($i=1;$i<=$N;$i++) {
&function_begin_B("_\$lazy${i}");
	&lea	("eax",&DWP(&label("OPENSSL_UplinkTable")));
	&push	($i);
	&push	("eax");
	&call	(&label("OPENSSL_Uplink"));
	&pop	("eax");
	&add	("esp",4);
	&jmp_ptr(&DWP(4*$i,"eax"));
&function_end_B("_\$lazy${i}");
}

&dataseg();
&align(4);
&set_label("OPENSSL_UplinkTable");
&data_word($N);
for ($i=1;$i<=$N;$i++) {
&data_word(&label("_\$lazy${i}"));
}
&asm_finish();
