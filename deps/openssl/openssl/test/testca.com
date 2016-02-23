$! TESTCA.COM
$
$	__arch = "VAX"
$	if f$getsyi("cpu") .ge. 128 then -
	   __arch = f$edit( f$getsyi( "ARCH_NAME"), "UPCASE")
$	if __arch .eqs. "" then __arch = "UNK"
$!
$       if (p1 .eqs. "64") then __arch = __arch+ "_64"
$
$	openssl = "mcr ''exe_dir'openssl"
$
$	SSLEAY_CONFIG="-config ""CAss.cnf"""
$
$	set noon
$	if f$search("demoCA.dir") .nes. ""
$	then
$	    @[-.util]deltree [.demoCA]*.*
$	    set file/prot=(S:RWED,O:RWED,G:RWED,W:RWED) demoCA.dir;*
$	    delete demoCA.dir;*
$	endif
$	set on
$	open/read sys$ca_input VMSca-response.1
$	@[-.apps]CA.com -input sys$ca_input -newca
$	close sys$ca_input
$	if $severity .ne. 1 then exit 3
$
$
$	SSLEAY_CONFIG="-config ""Uss.cnf"""
$	@[-.apps]CA.com -newreq
$	if $severity .ne. 1 then exit 3
$
$
$	SSLEAY_CONFIG="-config [-.apps]openssl-vms.cnf"
$	open/read sys$ca_input VMSca-response.2
$	@[-.apps]CA.com -input sys$ca_input -sign
$	close sys$ca_input
$	if $severity .ne. 1 then exit 3
$
$
$	@[-.apps]CA.com -verify newcert.pem
$	if $severity .ne. 1 then exit 3
$
$	set noon
$	@[-.util]deltree [.demoCA]*.*
$	set file/prot=(S:RWED,O:RWED,G:RWED,W:RWED) demoCA.dir;*
$	delete demoCA.dir;*
$	if f$search("newcert.pem") .nes. "" then delete newcert.pem;*
$	if f$search("newcert.pem") .nes. "" then delete newreq.pem;*
$	set on
$!	#usage: CA -newcert|-newreq|-newca|-sign|-verify
$
$	exit
