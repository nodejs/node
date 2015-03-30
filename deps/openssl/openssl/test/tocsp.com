$! TOCSP.COM
$
$	cmd = "mcr ''exe_dir'openssl"
$	ocspdir = "ocsp-tests"
$	! 17 December 2012 so we don't get certificate expiry errors.
$	check_time = "-attime 1355875200"
$
$ test_ocsp: subroutine
$	set noon
$	'cmd' base64 -d -in [.'ocspdir']'p1' -out f.d
$	'cmd' ocsp -respin f.d -partial_chain 'check_time' -
	      "-CAfile" [.'ocspdir']'p2' -verify_other [.'ocspdir']'p2' -
	      "-CApath" nul:
$	! when ocsp exits with 0, VMS severity becomes 1
$	! when ocsp exits with 1, VMS severity becomes 2
$	! See the definition of EXIT(n) in the VMS sextion in e_os.h
$	if $severity .ne. 'p3'+1 then exit 2 ! severity error
$	exit 1
$	endsubroutine
$
$	on error then exit 2
$	write sys$output "=== VALID OCSP RESPONSES ==="
$	write sys$output "NON-DELEGATED; Intermediate CA -> EE"
$	call test_ocsp ND1.ors ND1_Issuer_ICA.pem 0
$	write sys$output "NON-DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp ND2.ors ND2_Issuer_Root.pem 0
$	write sys$output "NON-DELEGATED; Root CA -> EE"
$	call test_ocsp ND3.ors ND3_Issuer_Root.pem 0
$	write sys$output "DELEGATED; Intermediate CA -> EE"
$	call test_ocsp D1.ors D1_Issuer_ICA.pem 0
$	write sys$output "DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp D2.ors D2_Issuer_Root.pem 0
$	write sys$output "DELEGATED; Root CA -> EE"
$	call test_ocsp D3.ors D3_Issuer_Root.pem 0
$
$	write sys$output "=== INVALID SIGNATURE on the OCSP RESPONSE ==="
$	write sys$output "NON-DELEGATED; Intermediate CA -> EE"
$	call test_ocsp ISOP_ND1.ors ND1_Issuer_ICA.pem 1
$	write sys$output "NON-DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp ISOP_ND2.ors ND2_Issuer_Root.pem 1
$	write sys$output "NON-DELEGATED; Root CA -> EE"
$	call test_ocsp ISOP_ND3.ors ND3_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Intermediate CA -> EE"
$	call test_ocsp ISOP_D1.ors D1_Issuer_ICA.pem 1
$	write sys$output "DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp ISOP_D2.ors D2_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Root CA -> EE"
$	call test_ocsp ISOP_D3.ors D3_Issuer_Root.pem 1
$
$	write sys$output "=== WRONG RESPONDERID in the OCSP RESPONSE ==="
$	write sys$output "NON-DELEGATED; Intermediate CA -> EE"
$	call test_ocsp WRID_ND1.ors ND1_Issuer_ICA.pem 1
$	write sys$output "NON-DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp WRID_ND2.ors ND2_Issuer_Root.pem 1
$	write sys$output "NON-DELEGATED; Root CA -> EE"
$	call test_ocsp WRID_ND3.ors ND3_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Intermediate CA -> EE"
$	call test_ocsp WRID_D1.ors D1_Issuer_ICA.pem 1
$	write sys$output "DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp WRID_D2.ors D2_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Root CA -> EE"
$	call test_ocsp WRID_D3.ors D3_Issuer_Root.pem 1
$
$	write sys$output "=== WRONG ISSUERNAMEHASH in the OCSP RESPONSE ==="
$	write sys$output "NON-DELEGATED; Intermediate CA -> EE"
$	call test_ocsp WINH_ND1.ors ND1_Issuer_ICA.pem 1
$	write sys$output "NON-DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp WINH_ND2.ors ND2_Issuer_Root.pem 1
$	write sys$output "NON-DELEGATED; Root CA -> EE"
$	call test_ocsp WINH_ND3.ors ND3_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Intermediate CA -> EE"
$	call test_ocsp WINH_D1.ors D1_Issuer_ICA.pem 1
$	write sys$output "DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp WINH_D2.ors D2_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Root CA -> EE"
$	call test_ocsp WINH_D3.ors D3_Issuer_Root.pem 1
$
$	write sys$output "=== WRONG ISSUERKEYHASH in the OCSP RESPONSE ==="
$	write sys$output "NON-DELEGATED; Intermediate CA -> EE"
$	call test_ocsp WIKH_ND1.ors ND1_Issuer_ICA.pem 1
$	write sys$output "NON-DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp WIKH_ND2.ors ND2_Issuer_Root.pem 1
$	write sys$output "NON-DELEGATED; Root CA -> EE"
$	call test_ocsp WIKH_ND3.ors ND3_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Intermediate CA -> EE"
$	call test_ocsp WIKH_D1.ors D1_Issuer_ICA.pem 1
$	write sys$output "DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp WIKH_D2.ors D2_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Root CA -> EE"
$	call test_ocsp WIKH_D3.ors D3_Issuer_Root.pem 1
$
$	write sys$output "=== WRONG KEY in the DELEGATED OCSP SIGNING CERTIFICATE ==="
$	write sys$output "DELEGATED; Intermediate CA -> EE"
$	call test_ocsp WKDOSC_D1.ors D1_Issuer_ICA.pem 1
$	write sys$output "DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp WKDOSC_D2.ors D2_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Root CA -> EE"
$	call test_ocsp WKDOSC_D3.ors D3_Issuer_Root.pem 1
$
$	write sys$output "=== INVALID SIGNATURE on the DELEGATED OCSP SIGNING CERTIFICATE ==="
$	write sys$output "DELEGATED; Intermediate CA -> EE"
$	call test_ocsp ISDOSC_D1.ors D1_Issuer_ICA.pem 1
$	write sys$output "DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp ISDOSC_D2.ors D2_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Root CA -> EE"
$	call test_ocsp ISDOSC_D3.ors D3_Issuer_Root.pem 1
$
$	write sys$output "=== WRONG SUBJECT NAME in the ISSUER CERTIFICATE ==="
$	write sys$output "NON-DELEGATED; Intermediate CA -> EE"
$	call test_ocsp ND1.ors WSNIC_ND1_Issuer_ICA.pem 1
$	write sys$output "NON-DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp ND2.ors WSNIC_ND2_Issuer_Root.pem 1
$	write sys$output "NON-DELEGATED; Root CA -> EE"
$	call test_ocsp ND3.ors WSNIC_ND3_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Intermediate CA -> EE"
$	call test_ocsp D1.ors WSNIC_D1_Issuer_ICA.pem 1
$	write sys$output "DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp D2.ors WSNIC_D2_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Root CA -> EE"
$	call test_ocsp D3.ors WSNIC_D3_Issuer_Root.pem 1
$
$	write sys$output "=== WRONG KEY in the ISSUER CERTIFICATE ==="
$	write sys$output "NON-DELEGATED; Intermediate CA -> EE"
$	call test_ocsp ND1.ors WKIC_ND1_Issuer_ICA.pem 1
$	write sys$output "NON-DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp ND2.ors WKIC_ND2_Issuer_Root.pem 1
$	write sys$output "NON-DELEGATED; Root CA -> EE"
$	call test_ocsp ND3.ors WKIC_ND3_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Intermediate CA -> EE"
$	call test_ocsp D1.ors WKIC_D1_Issuer_ICA.pem 1
$	write sys$output "DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp D2.ors WKIC_D2_Issuer_Root.pem 1
$	write sys$output "DELEGATED; Root CA -> EE"
$	call test_ocsp D3.ors WKIC_D3_Issuer_Root.pem 1
$
$	write sys$output "=== INVALID SIGNATURE on the ISSUER CERTIFICATE ==="
$	!# Expect success, because we're explicitly trusting the issuer certificate.
$	write sys$output "NON-DELEGATED; Intermediate CA -> EE"
$	call test_ocsp ND1.ors ISIC_ND1_Issuer_ICA.pem 0
$	write sys$output "NON-DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp ND2.ors ISIC_ND2_Issuer_Root.pem 0
$	write sys$output "NON-DELEGATED; Root CA -> EE"
$	call test_ocsp ND3.ors ISIC_ND3_Issuer_Root.pem 0
$	write sys$output "DELEGATED; Intermediate CA -> EE"
$	call test_ocsp D1.ors ISIC_D1_Issuer_ICA.pem 0
$	write sys$output "DELEGATED; Root CA -> Intermediate CA"
$	call test_ocsp D2.ors ISIC_D2_Issuer_Root.pem 0
$	write sys$output "DELEGATED; Root CA -> EE"
$	call test_ocsp D3.ors ISIC_D3_Issuer_Root.pem 0
$
$	write sys$output "ALL OCSP TESTS SUCCESSFUL"
$	exit 1
