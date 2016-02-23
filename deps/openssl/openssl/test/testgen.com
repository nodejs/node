$! TESTGEN.COM
$
$	__arch = "VAX"
$	if f$getsyi("cpu") .ge. 128 then -
	   __arch = f$edit( f$getsyi( "ARCH_NAME"), "UPCASE")
$	if __arch .eqs. "" then __arch = "UNK"
$	if (p1 .eqs. 64) then __arch = __arch+ "_64"
$	exe_dir = "sys$disk:[-.''__arch'.exe.apps]"
$
$	T = "testcert"
$	KEY = 512
$	CA = "[-.certs]testca.pem"
$
$	set noon
$	if f$search(T+".1;*") .nes. "" then delete 'T'.1;*
$	if f$search(T+".2;*") .nes. "" then delete 'T'.2;*
$	if f$search(T+".key;*") .nes. "" then delete 'T'.key;*
$	set on
$
$	write sys$output "generating certificate request"
$
$	append/new nl: .rnd
$	open/append random_file .rnd
$	write random_file -
	 "string to make the random number generator think it has entropy"
$	close random_file
$
$	set noon
$	define/user sys$output nla0:
$	mcr 'exe_dir'openssl no-rsa
$	save_severity=$SEVERITY
$	set on
$	if save_severity
$	then
$	    req_new="-newkey dsa:[-.apps]dsa512.pem"
$	else
$	    req_new="-new"
$	    write sys$output -
	     "There should be a 2 sequences of .'s and some +'s."
$	    write sys$output -
	     "There should not be more that at most 80 per line"
$	endif
$
$	write sys$output "This could take some time."
$
$	mcr 'exe_dir'openssl req -config test.cnf 'req_new' -out testreq.pem
$	if $severity .ne. 1
$	then
$	    write sys$output "problems creating request"
$	    exit 3
$	endif
$
$	mcr 'exe_dir'openssl req -config test.cnf -verify -in testreq.pem -noout
$	if $severity .ne. 1
$	then
$	    write sys$output "signature on req is wrong"
$	    exit 3
$	endif
