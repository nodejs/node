$! TESTSSL.COM
$
$	__arch = "VAX"
$	if f$getsyi("cpu") .ge. 128 then -
	   __arch = f$edit( f$getsyi( "ARCH_NAME"), "UPCASE")
$	if __arch .eqs. "" then __arch = "UNK"
$!
$	if (p4 .eqs. "64") then __arch = __arch+ "_64"
$!
$	texe_dir = "sys$disk:[-.''__arch'.exe.test]"
$	exe_dir = "sys$disk:[-.''__arch'.exe.apps]"
$
$	if p1 .eqs. ""
$	then
$	    key="[-.apps]server.pem"
$	else
$	    key=p1
$	endif
$	if p2 .eqs. ""
$	then
$	    cert="[-.apps]server.pem"
$	else
$	    cert=p2
$	endif
$	ssltest = "mcr ''texe_dir'ssltest -key ''key'"+ -
	 " -cert ''cert' -c_key ''key' -c_cert ''cert'"
$!
$	set noon
$	define/user sys$output testssl-x509-output.
$	define/user sys$error nla0:
$	mcr 'exe_dir'openssl x509 -in 'cert' -text -noout
$	define/user sys$error nla0:
$	search/output=nla0: testssl-x509-output. "DSA Public Key"/exact
$	if $severity .eq. 1
$	then
$	    dsa_cert = "YES"
$	else
$	    dsa_cert = "NO"
$	endif
$	delete testssl-x509-output.;*
$
$	if p3 .eqs. ""
$	then
$	    copy/concatenate [-.certs]*.pem certs.tmp
$	    CA = """-CAfile"" certs.tmp"
$	else
$	    CA = """-CAfile"" "+p3
$	endif
$
$!###########################################################################
$
$	write sys$output "test sslv2"
$	'ssltest' -ssl2
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv2 with server authentication"
$	'ssltest' -ssl2 -server_auth 'CA'
$	if $severity .ne. 1 then goto exit3
$
$	if .not. dsa_cert
$	then
$	    write sys$output "test sslv2 with client authentication"
$	    'ssltest' -ssl2 -client_auth 'CA'
$	    if $severity .ne. 1 then goto exit3
$
$	    write sys$output "test sslv2 with both client and server authentication"
$	    'ssltest' -ssl2 -server_auth -client_auth 'CA'
$	    if $severity .ne. 1 then goto exit3
$	endif
$
$	write sys$output "test sslv3"
$	'ssltest' -ssl3
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv3 with server authentication"
$	'ssltest' -ssl3 -server_auth 'CA'
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv3 with client authentication"
$	'ssltest' -ssl3 -client_auth 'CA'
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv3 with both client and server authentication"
$	'ssltest' -ssl3 -server_auth -client_auth 'CA'
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv2/sslv3"
$	'ssltest'
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv2/sslv3 with server authentication"
$	'ssltest' -server_auth 'CA'
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv2/sslv3 with client authentication"
$	'ssltest' -client_auth 'CA'
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv2/sslv3 with both client and server authentication"
$	'ssltest' -server_auth -client_auth 'CA'
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv2 via BIO pair"
$	'ssltest' -bio_pair -ssl2 
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv2 with server authentication via BIO pair"
$	'ssltest' -bio_pair -ssl2 -server_auth 'CA' 
$	if $severity .ne. 1 then goto exit3
$
$	if .not. dsa_cert
$	then
$	    write sys$output "test sslv2 with client authentication via BIO pair"
$	    'ssltest' -bio_pair -ssl2 -client_auth 'CA' 
$	    if $severity .ne. 1 then goto exit3
$
$	    write sys$output "test sslv2 with both client and server authentication via BIO pair"
$	    'ssltest' -bio_pair -ssl2 -server_auth -client_auth 'CA' 
$	    if $severity .ne. 1 then goto exit3
$	endif
$
$	write sys$output "test sslv3 via BIO pair"
$	'ssltest' -bio_pair -ssl3 
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv3 with server authentication via BIO pair"
$	'ssltest' -bio_pair -ssl3 -server_auth 'CA' 
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv3 with client authentication via BIO pair"
$	'ssltest' -bio_pair -ssl3 -client_auth 'CA' 
$	if $severity .ne. 1 then goto exit3
 
$	write sys$output "test sslv3 with both client and server authentication via BIO pair"
$	'ssltest' -bio_pair -ssl3 -server_auth -client_auth 'CA' 
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv2/sslv3 via BIO pair"
$	'ssltest' 
$	if $severity .ne. 1 then goto exit3
$
$	if .not. dsa_cert
$	then
$	    write sys$output "test sslv2/sslv3 w/o DHE via BIO pair"
$	    'ssltest' -bio_pair -no_dhe
$	    if $severity .ne. 1 then goto exit3
$	endif
$
$	write sys$output "test sslv2/sslv3 with 1024 bit DHE via BIO pair"
$	'ssltest' -bio_pair -dhe1024dsa -v
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv2/sslv3 with server authentication"
$	'ssltest' -bio_pair -server_auth 'CA' 
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv2/sslv3 with client authentication via BIO pair"
$	'ssltest' -bio_pair -client_auth 'CA' 
$	if $severity .ne. 1 then goto exit3
$
$	write sys$output "test sslv2/sslv3 with both client and server authentication via BIO pair"
$	'ssltest' -bio_pair -server_auth -client_auth 'CA' 
$	if $severity .ne. 1 then goto exit3
$
$!###########################################################################
$
$	define/user sys$output nla0:
$	mcr 'exe_dir'openssl no-rsa
$	no_rsa=$SEVERITY
$	define/user sys$output nla0:
$	mcr 'exe_dir'openssl no-dh
$	no_dh=$SEVERITY
$
$	if no_dh
$	then
$	    write sys$output "skipping anonymous DH tests"
$	else
$	    write sys$output "test tls1 with 1024bit anonymous DH, multiple handshakes"
$	    'ssltest' -v -bio_pair -tls1 -cipher "ADH" -dhe1024dsa -num 10 -f -time
$	    if $severity .ne. 1 then goto exit3
$	endif
$
$	if no_rsa
$	then
$	    write sys$output "skipping RSA tests"
$	else
$	    write sys$output "test tls1 with 1024bit RSA, no DHE, multiple handshakes"
$	    mcr 'texe_dir'ssltest -v -bio_pair -tls1 -cert [-.apps]server2.pem -no_dhe -num 10 -f -time
$	    if $severity .ne. 1 then goto exit3
$
$	    if no_dh
$	    then
$		write sys$output "skipping RSA+DHE tests"
$	    else
$		write sys$output "test tls1 with 1024bit RSA, 1024bit DHE, multiple handshakes"
$		mcr 'texe_dir'ssltest -v -bio_pair -tls1 -cert [-.apps]server2.pem -dhe1024dsa -num 10 -f -time
$		if $severity .ne. 1 then goto exit3
$	    endif
$	endif
$
$	RET = 1
$	goto exit
$ exit3:
$	RET = 3
$ exit:
$	if p3 .eqs. "" then delete certs.tmp;*
$	set on
$	exit 'RET'
