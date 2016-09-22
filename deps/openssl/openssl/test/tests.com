$! TESTS.COM  --  Performs the necessary tests
$!
$! P1	tests to be performed.  Empty means all.
$! P2	Pointer size: "", "32", or "64".
$!
$! Announce/identify.
$!
$	proc = f$environment( "procedure")
$	write sys$output "@@@ "+ -
	 f$parse( proc, , , "name")+ f$parse( proc, , , "type")
$!
$	__proc = f$element(0,";",f$environment("procedure"))
$	__here = f$parse(f$parse("A.;",__proc) - "A.;","[]A.;") - "A.;"
$	__save_default = f$environment("default")
$	__arch = "VAX"
$	if f$getsyi("cpu") .ge. 128 then -
	   __arch = f$edit( f$getsyi( "ARCH_NAME"), "UPCASE")
$	if __arch .eqs. "" then __arch = "UNK"
$!
$	__archd = __arch
$       pointer_size = ""
$	if (p2 .eq. "64")
$	then
$	  pointer_size = "64"
$	  __archd = __arch+ "_64"
$	endif
$!
$	texe_dir := sys$disk:[-.'__archd'.exe.test]
$	exe_dir := sys$disk:[-.'__archd'.exe.apps]
$
$	set default '__here'
$
$       ROOT = F$PARSE("sys$disk:[-]A.;0",,,,"SYNTAX_ONLY,NO_CONCEAL") - "A.;0"
$       ROOT_DEV = F$PARSE(ROOT,,,"DEVICE","SYNTAX_ONLY")
$       ROOT_DIR = F$PARSE(ROOT,,,"DIRECTORY","SYNTAX_ONLY") -
                   - ".][000000" - "[000000." - "][" - "[" - "]"
$       ROOT = ROOT_DEV + "[" + ROOT_DIR
$       DEFINE/NOLOG SSLROOT 'ROOT'.APPS.] /TRANS=CONC
$	openssl_conf := sslroot:[000000]openssl-vms.cnf
$
$	on control_y then goto exit
$	on error then goto exit
$
$	if p1 .nes. ""
$	then
$	    tests = p1
$	else
$! NOTE: This list reflects the list of dependencies following the
$! "alltests" target in Makefile.  This should make it easy to see
$! if there's a difference that needs to be taken care of.
$	    tests := -
	test_des,test_idea,test_sha,test_md4,test_md5,test_hmac,-
	test_md2,test_mdc2,test_wp,-
	test_rmd,test_rc2,test_rc4,test_rc5,test_bf,test_cast,test_aes,-
	test_rand,test_bn,test_ec,test_ecdsa,test_ecdh,-
	test_enc,test_x509,test_rsa,test_crl,test_sid,-
	test_gen,test_req,test_pkcs7,test_verify,test_dh,test_dsa,-
	test_ss,test_ca,test_engine,test_evp,test_evp_extra,test_ssl,test_tsa,test_ige,-
	test_jpake,test_srp,test_cms,test_ocsp,test_v3name,test_heartbeat,-
	test_constant_time,test_verify_extra,test_clienthello,test_sslv2conftest,test_dtls
$	endif
$	tests = f$edit(tests,"COLLAPSE")
$
$	BNTEST :=	bntest
$	ECTEST :=	ectest
$	ECDSATEST :=	ecdsatest
$	ECDHTEST :=	ecdhtest
$	EXPTEST :=	exptest
$	IDEATEST :=	ideatest
$	SHATEST :=	shatest
$	SHA1TEST :=	sha1test
$	SHA256TEST :=	sha256t
$	SHA512TEST :=	sha512t
$	MDC2TEST :=	mdc2test
$	RMDTEST :=	rmdtest
$	MD2TEST :=	md2test
$	MD4TEST :=	md4test
$	MD5TEST :=	md5test
$	HMACTEST :=	hmactest
$	WPTEST :=	wp_test
$	RC2TEST :=	rc2test
$	RC4TEST :=	rc4test
$	RC5TEST :=	rc5test
$	BFTEST :=	bftest
$	CASTTEST :=	casttest
$	DESTEST :=	destest
$	RANDTEST :=	randtest
$	DHTEST :=	dhtest
$	DSATEST :=	dsatest
$	METHTEST :=	methtest
$	SSLTEST :=	ssltest
$	RSATEST :=	rsa_test
$	ENGINETEST :=	enginetest
$	EVPTEST :=	evp_test
$	EVPEXTRATEST :=	evp_extra_test
$	IGETEST :=	igetest
$	JPAKETEST :=	jpaketest
$	SRPTEST :=	srptest
$	V3NAMETEST :=	v3nametest
$	ASN1TEST :=	asn1test
$	HEARTBEATTEST :=	heartbeat_test
$	CONSTTIMETEST :=	constant_time_test
$	VERIFYEXTRATEST :=	verify_extra_test
$	CLIENTHELLOTEST :=	clienthellotest
$	SSLV2CONFTEST := 	sslv2conftest
$	DTLSTEST :=	dtlstest
$!
$	tests_i = 0
$ loop_tests:
$	tests_e = f$element(tests_i,",",tests)
$	tests_i = tests_i + 1
$	if tests_e .eqs. "," then goto exit
$	write sys$output "---> ''tests_e'"
$	gosub 'tests_e'
$	goto loop_tests
$
$ test_evp:
$	mcr 'texe_dir''evptest' 'ROOT'.CRYPTO.EVP]evptests.txt
$	return
$ test_evp_extra:
$	mcr 'texe_dir''evpextratest'
$	return
$ test_des:
$	mcr 'texe_dir''destest'
$	return
$ test_idea:
$	mcr 'texe_dir''ideatest'
$	return
$ test_sha:
$	mcr 'texe_dir''shatest'
$	mcr 'texe_dir''sha1test'
$	mcr 'texe_dir''sha256test'
$	mcr 'texe_dir''sha512test'
$	return
$ test_mdc2:
$	mcr 'texe_dir''mdc2test'
$	return
$ test_md5:
$	mcr 'texe_dir''md5test'
$	return
$ test_md4:
$	mcr 'texe_dir''md4test'
$	return
$ test_hmac:
$	mcr 'texe_dir''hmactest'
$	return
$ test_wp:
$	mcr 'texe_dir''wptest'
$	return
$ test_md2:
$	mcr 'texe_dir''md2test'
$	return
$ test_rmd:
$	mcr 'texe_dir''rmdtest'
$	return
$ test_bf:
$	mcr 'texe_dir''bftest'
$	return
$ test_cast:
$	mcr 'texe_dir''casttest'
$	return
$ test_rc2:
$	mcr 'texe_dir''rc2test'
$	return
$ test_rc4:
$	mcr 'texe_dir''rc4test'
$	return
$ test_rc5:
$	mcr 'texe_dir''rc5test'
$	return
$ test_rand:
$	mcr 'texe_dir''randtest'
$	return
$ test_enc:
$	@testenc.com 'pointer_size'
$	return
$ test_x509:
$	set noon
$	define sys$error test_x509.err
$	write sys$output "test normal x509v1 certificate"
$	@tx509.com "" 'pointer_size'
$	write sys$output "test first x509v3 certificate"
$	@tx509.com v3-cert1.pem 'pointer_size'
$	write sys$output "test second x509v3 certificate"
$	@tx509.com v3-cert2.pem 'pointer_size'
$	deassign sys$error
$	set on
$	return
$ test_rsa:
$	set noon
$	define sys$error test_rsa.err
$	@trsa.com "" 'pointer_size'
$	deassign sys$error
$	mcr 'texe_dir''rsatest'
$	set on
$	return
$ test_crl:
$	set noon
$	define sys$error test_crl.err
$	@tcrl.com "" 'pointer_size'
$	deassign sys$error
$	set on
$	return
$ test_sid:
$	set noon
$	define sys$error test_sid.err
$	@tsid.com "" 'pointer_size'
$	deassign sys$error
$	set on
$	return
$ test_req:
$	set noon
$	define sys$error test_req.err
$	@treq.com "" 'pointer_size'
$	@treq.com testreq2.pem 'pointer_size'
$	deassign sys$error
$	set on
$	return
$ test_pkcs7:
$	set noon
$	define sys$error test_pkcs7.err
$	@tpkcs7.com "" 'pointer_size'
$	@tpkcs7d.com "" 'pointer_size'
$	deassign sys$error
$	set on
$	return
$ test_bn:
$	write sys$output -
	      "starting big number library test, could take a while..."
$	set noon
$	define sys$error test_bn.err
$	define sys$output test_bn.out
$	@ bctest.com
$	status = $status
$	deassign sys$error
$	deassign sys$output
$	set on
$	if (status)
$	then
$	    create /fdl = sys$input bntest-vms.tmp
FILE
	ORGANIZATION	sequential
RECORD
	FORMAT		stream_lf
$	    define /user_mode sys$output bntest-vms.tmp
$	    mcr 'texe_dir''bntest'
$	    define /user_mode sys$input bntest-vms.tmp
$	    define /user_mode sys$output bntest-vms.out
$	    bc
$	    @ bntest.com bntest-vms.out
$	    status = $status
$	    if (status)
$	    then
$		delete bntest-vms.out;*
$		delete bntest-vms.tmp;*
$	    endif
$	else
$	    create /fdl = sys$input bntest-vms.sh
FILE
	ORGANIZATION	sequential
RECORD
	FORMAT		stream_lf
$	    open /append bntest_file bntest-vms.sh
$	    type /output = bntest_file sys$input:
<< __FOO__ sh -c "`sh ./bctest`" | perl -e '$i=0; while (<STDIN>) {if (/^test (.*)/) {print STDERR "\nverify $1";} elsif (!/^0$/) {die "\nFailed! bc: $_";} else {print STDERR "."; $i++;}} print STDERR "\n$i tests passed\n"'
$	    define /user_mode sys$output bntest-vms.tmp
$	    mcr 'texe_dir''bntest'
$	    copy bntest-vms.tmp bntest_file
$	    delete bntest-vms.tmp;*
$	    type /output = bntest_file sys$input:
__FOO__
$	    close bntest_file
$	    write sys$output "-- copy the [.test]bntest-vms.sh and [.test]bctest files to a Unix system and"
$	    write sys$output "-- run bntest-vms.sh through sh or bash to verify that the bignum operations"
$	    write sys$output "-- went well."
$	    write sys$output ""
$	endif
$	write sys$output "test a^b%c implementations"
$	mcr 'texe_dir''exptest'
$	return
$ test_ec:
$	write sys$output "test elliptic curves"
$	mcr 'texe_dir''ectest'
$	return
$ test_ecdsa:
$	write sys$output "test ecdsa"
$	mcr 'texe_dir''ecdsatest'
$	return
$ test_ecdh:
$	write sys$output "test ecdh"
$	mcr 'texe_dir''ecdhtest'
$	return
$ test_verify:
$	write sys$output "The following command should have some OK's and some failures"
$	write sys$output "There are definitly a few expired certificates"
$	@tverify.com 'pointer_size'
$	return
$ test_dh:
$	write sys$output "Generate a set of DH parameters"
$	mcr 'texe_dir''dhtest'
$	return
$ test_dsa:
$	write sys$output "Generate a set of DSA parameters"
$	mcr 'texe_dir''dsatest'
$	return
$ test_gen:
$	write sys$output "Generate and verify a certificate request"
$	@testgen.com 'pointer_size'
$	return
$ maybe_test_ss:
$	testss_RDT = f$cvtime(f$file_attributes("testss.com","RDT"))
$	if f$cvtime(f$file_attributes("keyU.ss","RDT")) .les. testss_RDT then -
		goto test_ss
$	if f$cvtime(f$file_attributes("certU.ss","RDT")) .les. testss_RDT then -
		goto test_ss
$	if f$cvtime(f$file_attributes("certCA.ss","RDT")) .les. testss_RDT then -
		goto test_ss
$	return
$ test_ss:
$	write sys$output "Generate and certify a test certificate"
$	@testss.com 'pointer_size'
$	return
$ test_engine: 
$	write sys$output "Manipulate the ENGINE structures"
$	mcr 'texe_dir''enginetest'
$	return
$ test_ssl:
$	write sys$output "test SSL protocol"
$	gosub maybe_test_ss
$	@testssl.com keyU.ss certU.ss certCA.ss 'pointer_size'
$	return
$ test_ca:
$	set noon
$	define /user_mode sys$output test_ca.out
$	mcr 'exe_dir'openssl no-rsa
$	save_severity=$SEVERITY
$	set on
$	if save_severity
$	then
$	    write sys$output "skipping CA.com test -- requires RSA"
$	else
$	    write sys$output "Generate and certify a test certificate via the 'ca' program"
$	    @testca.com 'pointer_size'
$	endif
$	return
$ test_aes: 
$!	write sys$output "test AES"
$!	!mcr 'texe_dir''aestest'
$	return
$ test_tsa:
$	set noon
$	define /user_mode sys$output nla0:
$	mcr 'exe_dir'openssl no-rsa
$	save_severity=$SEVERITY
$	set on
$	if save_severity
$	then
$	    write sys$output "skipping testtsa.com test -- requires RSA"
$	else
$	    @testtsa.com "" "" "" 'pointer_size'
$	endif
$	return
$ test_ige: 
$	write sys$output "Test IGE mode"
$	mcr 'texe_dir''igetest'
$	return
$ test_jpake: 
$	write sys$output "Test JPAKE"
$	mcr 'texe_dir''jpaketest'
$	return
$ test_cms:
$	write sys$output "CMS consistency test"
$	! Define the logical name used to find openssl.exe in the perl script.
$	define /user_mode osslx 'exe_dir'
$	perl CMS-TEST.PL
$	return
$ test_srp: 
$	write sys$output "Test SRP"
$	mcr 'texe_dir''srptest'
$	return
$ test_ocsp:
$	write sys$output "Test OCSP"
$	@tocsp.com
$	return
$ test_v3name:
$       write sys$output "Test V3NAME"
$       mcr 'texe_dir''v3nametest'
$       return
$ test_heartbeat:
$       write sys$output "Test HEARTBEAT"
$       mcr 'texe_dir''heartbeattest'
$       return
$ test_constant_time:
$       write sys$output "Test constant time utilities"
$       mcr 'texe_dir''consttimetest'
$       return
$ test_verify_extra:
$	write sys$output "''START' test_verify_extra"
$	mcr 'texe_dir''verifyextratest'
$       return
$ test_clienthello:
$	write sys$output "''START' test_clienthello"
$	mcr 'texe_dir''clienthellotest'
$       return
$ test_sslv2conftest:
$	write sys$output "''START' test_sslv2conftest"
$	mcr 'texe_dir''sslv2conftest'
$       return
$ test_dtls:
$	write sys$output "''START' test_dtls"
$	mcr 'texe_dir''dtlstest' 'ROOT'.APPS]server.pem 'ROOT'.APPS]server.pem
$	return
$
$ exit:
$	on error then goto exit2 ! In case openssl.exe didn't build.
$	mcr 'exe_dir'openssl version -a
$ exit2: 
$	set default '__save_default'
$	deassign sslroot
$	exit
