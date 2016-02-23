#!/bin/sh

cmd='../util/shlib_wrap.sh ../apps/openssl'
ocspdir="ocsp-tests"
# 17 December 2012 so we don't get certificate expiry errors.
check_time="-attime 1355875200"

test_ocsp () {

	$cmd base64 -d -in $ocspdir/$1 | \
		$cmd ocsp -respin - -partial_chain $check_time -trusted_first \
		-CAfile $ocspdir/$2 -verify_other $ocspdir/$2 -CApath /dev/null
	[ $? != $3 ] && exit 1
}


echo "=== VALID OCSP RESPONSES ==="
echo "NON-DELEGATED; Intermediate CA -> EE"
test_ocsp ND1.ors ND1_Issuer_ICA.pem 0
echo "NON-DELEGATED; Root CA -> Intermediate CA"
test_ocsp ND2.ors ND2_Issuer_Root.pem 0
echo "NON-DELEGATED; Root CA -> EE"
test_ocsp ND3.ors ND3_Issuer_Root.pem 0
echo "DELEGATED; Intermediate CA -> EE"
test_ocsp D1.ors D1_Issuer_ICA.pem 0
echo "DELEGATED; Root CA -> Intermediate CA"
test_ocsp D2.ors D2_Issuer_Root.pem 0
echo "DELEGATED; Root CA -> EE"
test_ocsp D3.ors D3_Issuer_Root.pem 0

echo "=== INVALID SIGNATURE on the OCSP RESPONSE ==="
echo "NON-DELEGATED; Intermediate CA -> EE"
test_ocsp ISOP_ND1.ors ND1_Issuer_ICA.pem 1
echo "NON-DELEGATED; Root CA -> Intermediate CA"
test_ocsp ISOP_ND2.ors ND2_Issuer_Root.pem 1
echo "NON-DELEGATED; Root CA -> EE"
test_ocsp ISOP_ND3.ors ND3_Issuer_Root.pem 1
echo "DELEGATED; Intermediate CA -> EE"
test_ocsp ISOP_D1.ors D1_Issuer_ICA.pem 1
echo "DELEGATED; Root CA -> Intermediate CA"
test_ocsp ISOP_D2.ors D2_Issuer_Root.pem 1
echo "DELEGATED; Root CA -> EE"
test_ocsp ISOP_D3.ors D3_Issuer_Root.pem 1

echo "=== WRONG RESPONDERID in the OCSP RESPONSE ==="
echo "NON-DELEGATED; Intermediate CA -> EE"
test_ocsp WRID_ND1.ors ND1_Issuer_ICA.pem 1
echo "NON-DELEGATED; Root CA -> Intermediate CA"
test_ocsp WRID_ND2.ors ND2_Issuer_Root.pem 1
echo "NON-DELEGATED; Root CA -> EE"
test_ocsp WRID_ND3.ors ND3_Issuer_Root.pem 1
echo "DELEGATED; Intermediate CA -> EE"
test_ocsp WRID_D1.ors D1_Issuer_ICA.pem 1
echo "DELEGATED; Root CA -> Intermediate CA"
test_ocsp WRID_D2.ors D2_Issuer_Root.pem 1
echo "DELEGATED; Root CA -> EE"
test_ocsp WRID_D3.ors D3_Issuer_Root.pem 1

echo "=== WRONG ISSUERNAMEHASH in the OCSP RESPONSE ==="
echo "NON-DELEGATED; Intermediate CA -> EE"
test_ocsp WINH_ND1.ors ND1_Issuer_ICA.pem 1
echo "NON-DELEGATED; Root CA -> Intermediate CA"
test_ocsp WINH_ND2.ors ND2_Issuer_Root.pem 1
echo "NON-DELEGATED; Root CA -> EE"
test_ocsp WINH_ND3.ors ND3_Issuer_Root.pem 1
echo "DELEGATED; Intermediate CA -> EE"
test_ocsp WINH_D1.ors D1_Issuer_ICA.pem 1
echo "DELEGATED; Root CA -> Intermediate CA"
test_ocsp WINH_D2.ors D2_Issuer_Root.pem 1
echo "DELEGATED; Root CA -> EE"
test_ocsp WINH_D3.ors D3_Issuer_Root.pem 1

echo "=== WRONG ISSUERKEYHASH in the OCSP RESPONSE ==="
echo "NON-DELEGATED; Intermediate CA -> EE"
test_ocsp WIKH_ND1.ors ND1_Issuer_ICA.pem 1
echo "NON-DELEGATED; Root CA -> Intermediate CA"
test_ocsp WIKH_ND2.ors ND2_Issuer_Root.pem 1
echo "NON-DELEGATED; Root CA -> EE"
test_ocsp WIKH_ND3.ors ND3_Issuer_Root.pem 1
echo "DELEGATED; Intermediate CA -> EE"
test_ocsp WIKH_D1.ors D1_Issuer_ICA.pem 1
echo "DELEGATED; Root CA -> Intermediate CA"
test_ocsp WIKH_D2.ors D2_Issuer_Root.pem 1
echo "DELEGATED; Root CA -> EE"
test_ocsp WIKH_D3.ors D3_Issuer_Root.pem 1

echo "=== WRONG KEY in the DELEGATED OCSP SIGNING CERTIFICATE ==="
echo "DELEGATED; Intermediate CA -> EE"
test_ocsp WKDOSC_D1.ors D1_Issuer_ICA.pem 1
echo "DELEGATED; Root CA -> Intermediate CA"
test_ocsp WKDOSC_D2.ors D2_Issuer_Root.pem 1
echo "DELEGATED; Root CA -> EE"
test_ocsp WKDOSC_D3.ors D3_Issuer_Root.pem 1

echo "=== INVALID SIGNATURE on the DELEGATED OCSP SIGNING CERTIFICATE ==="
echo "DELEGATED; Intermediate CA -> EE"
test_ocsp ISDOSC_D1.ors D1_Issuer_ICA.pem 1
echo "DELEGATED; Root CA -> Intermediate CA"
test_ocsp ISDOSC_D2.ors D2_Issuer_Root.pem 1
echo "DELEGATED; Root CA -> EE"
test_ocsp ISDOSC_D3.ors D3_Issuer_Root.pem 1

echo "=== WRONG SUBJECT NAME in the ISSUER CERTIFICATE ==="
echo "NON-DELEGATED; Intermediate CA -> EE"
test_ocsp ND1.ors WSNIC_ND1_Issuer_ICA.pem 1
echo "NON-DELEGATED; Root CA -> Intermediate CA"
test_ocsp ND2.ors WSNIC_ND2_Issuer_Root.pem 1
echo "NON-DELEGATED; Root CA -> EE"
test_ocsp ND3.ors WSNIC_ND3_Issuer_Root.pem 1
echo "DELEGATED; Intermediate CA -> EE"
test_ocsp D1.ors WSNIC_D1_Issuer_ICA.pem 1
echo "DELEGATED; Root CA -> Intermediate CA"
test_ocsp D2.ors WSNIC_D2_Issuer_Root.pem 1
echo "DELEGATED; Root CA -> EE"
test_ocsp D3.ors WSNIC_D3_Issuer_Root.pem 1

echo "=== WRONG KEY in the ISSUER CERTIFICATE ==="
echo "NON-DELEGATED; Intermediate CA -> EE"
test_ocsp ND1.ors WKIC_ND1_Issuer_ICA.pem 1
echo "NON-DELEGATED; Root CA -> Intermediate CA"
test_ocsp ND2.ors WKIC_ND2_Issuer_Root.pem 1
echo "NON-DELEGATED; Root CA -> EE"
test_ocsp ND3.ors WKIC_ND3_Issuer_Root.pem 1
echo "DELEGATED; Intermediate CA -> EE"
test_ocsp D1.ors WKIC_D1_Issuer_ICA.pem 1
echo "DELEGATED; Root CA -> Intermediate CA"
test_ocsp D2.ors WKIC_D2_Issuer_Root.pem 1
echo "DELEGATED; Root CA -> EE"
test_ocsp D3.ors WKIC_D3_Issuer_Root.pem 1

echo "=== INVALID SIGNATURE on the ISSUER CERTIFICATE ==="
# Expect success, because we're explicitly trusting the issuer certificate.
echo "NON-DELEGATED; Intermediate CA -> EE"
test_ocsp ND1.ors ISIC_ND1_Issuer_ICA.pem 0
echo "NON-DELEGATED; Root CA -> Intermediate CA"
test_ocsp ND2.ors ISIC_ND2_Issuer_Root.pem 0
echo "NON-DELEGATED; Root CA -> EE"
test_ocsp ND3.ors ISIC_ND3_Issuer_Root.pem 0
echo "DELEGATED; Intermediate CA -> EE"
test_ocsp D1.ors ISIC_D1_Issuer_ICA.pem 0
echo "DELEGATED; Root CA -> Intermediate CA"
test_ocsp D2.ors ISIC_D2_Issuer_Root.pem 0
echo "DELEGATED; Root CA -> EE"
test_ocsp D3.ors ISIC_D3_Issuer_Root.pem 0

echo "ALL OCSP TESTS SUCCESSFUL"
exit 0
