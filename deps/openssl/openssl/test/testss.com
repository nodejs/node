$! TESTSS.COM
$
$	__arch = "VAX"
$	if f$getsyi("cpu") .ge. 128 then -
	   __arch = f$edit( f$getsyi( "ARCH_NAME"), "UPCASE")
$	if __arch .eqs. "" then __arch = "UNK"
$!
$	if (p1 .eqs. "64") then __arch = __arch+ "_64"
$!
$	exe_dir = "sys$disk:[-.''__arch'.exe.apps]"
$
$	digest="-md5"
$	reqcmd = "mcr ''exe_dir'openssl req"
$	x509cmd = "mcr ''exe_dir'openssl x509 ''digest'"
$	verifycmd = "mcr ''exe_dir'openssl verify"
$	dummycnf = "sys$disk:[-.apps]openssl-vms.cnf"
$
$	CAkey="""keyCA.ss"""
$	CAcert="""certCA.ss"""
$	CAreq="""reqCA.ss"""
$	CAconf="""CAss.cnf"""
$	CAreq2="""req2CA.ss"""	! temp
$
$	Uconf="""Uss.cnf"""
$	Ukey="""keyU.ss"""
$	Ureq="""reqU.ss"""
$	Ucert="""certU.ss"""
$
$	write sys$output ""
$	write sys$output "make a certificate request using 'req'"
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
$	endif
$
$	'reqcmd' -config 'CAconf' -out 'CAreq' -keyout 'CAkey' 'req_new' ! -out err.ss
$	if $severity .ne. 1
$	then
$		write sys$output "error using 'req' to generate a certificate request"
$		exit 3
$	endif
$	write sys$output ""
$	write sys$output "convert the certificate request into a self signed certificate using 'x509'"
$	define /user sys$output err.ss
$	'x509cmd' "-CAcreateserial" -in 'CAreq' -days 30 -req -out 'CAcert' -signkey 'CAkey'
$	if $severity .ne. 1
$	then
$		write sys$output "error using 'x509' to self sign a certificate request"
$		exit 3
$	endif
$
$	write sys$output ""
$	write sys$output "convert a certificate into a certificate request using 'x509'"
$	define /user sys$output err.ss
$	'x509cmd' -in 'CAcert' -x509toreq -signkey 'CAkey' -out 'CAreq2'
$	if $severity .ne. 1
$	then
$		write sys$output "error using 'x509' convert a certificate to a certificate request"
$		exit 3
$	endif
$
$	'reqcmd' -config 'dummycnf' -verify -in 'CAreq' -noout
$	if $severity .ne. 1
$	then
$		write sys$output "first generated request is invalid"
$		exit 3
$	endif
$
$	'reqcmd' -config 'dummycnf' -verify -in 'CAreq2' -noout
$	if $severity .ne. 1
$	then
$		write sys$output "second generated request is invalid"
$		exit 3
$	endif
$
$	'verifycmd' "-CAfile" 'CAcert' 'CAcert'
$	if $severity .ne. 1
$	then
$		write sys$output "first generated cert is invalid"
$		exit 3
$	endif
$
$	write sys$output ""
$	write sys$output "make another certificate request using 'req'"
$	define /user sys$output err.ss
$	'reqcmd' -config 'Uconf' -out 'Ureq' -keyout 'Ukey' 'req_new'
$	if $severity .ne. 1
$	then
$		write sys$output "error using 'req' to generate a certificate request"
$		exit 3
$	endif
$
$	write sys$output ""
$	write sys$output "sign certificate request with the just created CA via 'x509'"
$	define /user sys$output err.ss
$	'x509cmd' "-CAcreateserial" -in 'Ureq' -days 30 -req -out 'Ucert' "-CA" 'CAcert' "-CAkey" 'CAkey'
$	if $severity .ne. 1
$	then
$		write sys$output "error using 'x509' to sign a certificate request"
$		exit 3
$	endif
$
$	'verifycmd' "-CAfile" 'CAcert' 'Ucert'
$	write sys$output ""
$	write sys$output "Certificate details"
$	'x509cmd' -subject -issuer -startdate -enddate -noout -in 'Ucert'
$
$	write sys$output ""
$	write sys$output "The generated CA certificate is ",CAcert
$	write sys$output "The generated CA private key is ",CAkey
$
$	write sys$output "The generated user certificate is ",Ucert
$	write sys$output "The generated user private key is ",Ukey
$
$	if f$search("err.ss;*") .nes. "" then delete err.ss;*
