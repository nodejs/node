#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use POSIX;
use File::Spec::Functions qw/catfile/;
use File::Compare qw/compare_text/;
use OpenSSL::Test qw/:DEFAULT srctop_dir srctop_file/;
use OpenSSL::Test::Utils;

setup("test_cms");

plan skip_all => "CMS is not supported by this OpenSSL build"
    if disabled("cms");

my $smdir    = srctop_dir("test", "smime-certs");
my $smcont   = srctop_file("test", "smcont.txt");
my ($no_des, $no_dh, $no_dsa, $no_ec, $no_ec2m, $no_rc2, $no_zlib)
    = disabled qw/des dh dsa ec ec2m rc2 zlib/;

plan tests => 4;

my @smime_pkcs7_tests = (

    [ "signed content DER format, RSA key",
      [ "-sign", "-in", $smcont, "-outform", "DER", "-nodetach",
	"-certfile", catfile($smdir, "smroot.pem"),
	"-signer", catfile($smdir, "smrsa1.pem"), "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms", "-inform", "DER",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "signed detached content DER format, RSA key",
      [ "-sign", "-in", $smcont, "-outform", "DER",
	"-signer", catfile($smdir, "smrsa1.pem"), "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms", "-inform", "DER",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt",
	"-content", $smcont ]
    ],

    [ "signed content test streaming BER format, RSA",
      [ "-sign", "-in", $smcont, "-outform", "DER", "-nodetach",
	"-stream",
	"-signer", catfile($smdir, "smrsa1.pem"), "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms", "-inform", "DER",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "signed content DER format, DSA key",
      [ "-sign", "-in", $smcont, "-outform", "DER", "-nodetach",
	"-signer", catfile($smdir, "smdsa1.pem"), "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms", "-inform", "DER",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "signed detached content DER format, DSA key",
      [ "-sign", "-in", $smcont, "-outform", "DER",
	"-signer", catfile($smdir, "smdsa1.pem"), "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms", "-inform", "DER",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt",
	"-content", $smcont ]
    ],

    [ "signed detached content DER format, add RSA signer (with DSA existing)",
      [ "-resign", "-inform", "DER", "-in", "test.cms", "-outform", "DER",
	"-signer", catfile($smdir, "smrsa1.pem"), "-out", "test2.cms" ],
      [ "-verify", "-in", "test2.cms", "-inform", "DER",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt",
	"-content", $smcont ]
    ],

    [ "signed content test streaming BER format, DSA key",
      [ "-sign", "-in", $smcont, "-outform", "DER", "-nodetach",
	"-stream",
	"-signer", catfile($smdir, "smdsa1.pem"), "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms", "-inform", "DER",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "signed content test streaming BER format, 2 DSA and 2 RSA keys",
      [ "-sign", "-in", $smcont, "-outform", "DER", "-nodetach",
	"-signer", catfile($smdir, "smrsa1.pem"),
	"-signer", catfile($smdir, "smrsa2.pem"),
	"-signer", catfile($smdir, "smdsa1.pem"),
	"-signer", catfile($smdir, "smdsa2.pem"),
	"-stream", "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms", "-inform", "DER",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "signed content test streaming BER format, 2 DSA and 2 RSA keys, no attributes",
      [ "-sign", "-in", $smcont, "-outform", "DER", "-noattr", "-nodetach",
	"-signer", catfile($smdir, "smrsa1.pem"),
	"-signer", catfile($smdir, "smrsa2.pem"),
	"-signer", catfile($smdir, "smdsa1.pem"),
	"-signer", catfile($smdir, "smdsa2.pem"),
	"-stream", "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms", "-inform", "DER",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "signed content S/MIME format, RSA key SHA1",
      [ "-sign", "-in", $smcont, "-md", "sha1",
	"-certfile", catfile($smdir, "smroot.pem"),
	"-signer", catfile($smdir, "smrsa1.pem"), "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "signed content test streaming S/MIME format, 2 DSA and 2 RSA keys",
      [ "-sign", "-in", $smcont, "-nodetach",
	"-signer", catfile($smdir, "smrsa1.pem"),
	"-signer", catfile($smdir, "smrsa2.pem"),
	"-signer", catfile($smdir, "smdsa1.pem"),
	"-signer", catfile($smdir, "smdsa2.pem"),
	"-stream", "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "signed content test streaming multipart S/MIME format, 2 DSA and 2 RSA keys",
      [ "-sign", "-in", $smcont,
	"-signer", catfile($smdir, "smrsa1.pem"),
	"-signer", catfile($smdir, "smrsa2.pem"),
	"-signer", catfile($smdir, "smdsa1.pem"),
	"-signer", catfile($smdir, "smdsa2.pem"),
	"-stream", "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "enveloped content test streaming S/MIME format, DES, 3 recipients",
      [ "-encrypt", "-in", $smcont,
	"-stream", "-out", "test.cms",
	catfile($smdir, "smrsa1.pem"),
	catfile($smdir, "smrsa2.pem"),
	catfile($smdir, "smrsa3.pem") ],
      [ "-decrypt", "-recip", catfile($smdir, "smrsa1.pem"),
	"-in", "test.cms", "-out", "smtst.txt" ]
    ],

    [ "enveloped content test streaming S/MIME format, DES, 3 recipients, 3rd used",
      [ "-encrypt", "-in", $smcont,
	"-stream", "-out", "test.cms",
	catfile($smdir, "smrsa1.pem"),
	catfile($smdir, "smrsa2.pem"),
	catfile($smdir, "smrsa3.pem") ],
      [ "-decrypt", "-recip", catfile($smdir, "smrsa3.pem"),
	"-in", "test.cms", "-out", "smtst.txt" ]
    ],

    [ "enveloped content test streaming S/MIME format, DES, 3 recipients, key only used",
      [ "-encrypt", "-in", $smcont,
	"-stream", "-out", "test.cms",
	catfile($smdir, "smrsa1.pem"),
	catfile($smdir, "smrsa2.pem"),
	catfile($smdir, "smrsa3.pem") ],
      [ "-decrypt", "-inkey", catfile($smdir, "smrsa3.pem"),
	"-in", "test.cms", "-out", "smtst.txt" ]
    ],

    [ "enveloped content test streaming S/MIME format, AES-256 cipher, 3 recipients",
      [ "-encrypt", "-in", $smcont,
	"-aes256", "-stream", "-out", "test.cms",
	catfile($smdir, "smrsa1.pem"),
	catfile($smdir, "smrsa2.pem"),
	catfile($smdir, "smrsa3.pem") ],
      [ "-decrypt", "-recip", catfile($smdir, "smrsa1.pem"),
	"-in", "test.cms", "-out", "smtst.txt" ]
    ],

);

my @smime_cms_tests = (

    [ "signed content test streaming BER format, 2 DSA and 2 RSA keys, keyid",
      [ "-sign", "-in", $smcont, "-outform", "DER", "-nodetach", "-keyid",
	"-signer", catfile($smdir, "smrsa1.pem"),
	"-signer", catfile($smdir, "smrsa2.pem"),
	"-signer", catfile($smdir, "smdsa1.pem"),
	"-signer", catfile($smdir, "smdsa2.pem"),
	"-stream", "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms", "-inform", "DER",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "signed content test streaming PEM format, 2 DSA and 2 RSA keys",
      [ "-sign", "-in", $smcont, "-outform", "PEM", "-nodetach",
	"-signer", catfile($smdir, "smrsa1.pem"),
	"-signer", catfile($smdir, "smrsa2.pem"),
	"-signer", catfile($smdir, "smdsa1.pem"),
	"-signer", catfile($smdir, "smdsa2.pem"),
	"-stream", "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms", "-inform", "PEM",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "signed content MIME format, RSA key, signed receipt request",
      [ "-sign", "-in", $smcont, "-signer", catfile($smdir, "smrsa1.pem"), "-nodetach",
	"-receipt_request_to", "test\@openssl.org", "-receipt_request_all",
	"-out", "test.cms" ],
      [ "-verify", "-in", "test.cms",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "signed receipt MIME format, RSA key",
      [ "-sign_receipt", "-in", "test.cms",
	"-signer", catfile($smdir, "smrsa2.pem"),
	"-out", "test2.cms" ],
      [ "-verify_receipt", "test2.cms", "-in", "test.cms",
	"-CAfile", catfile($smdir, "smroot.pem") ]
    ],

    [ "enveloped content test streaming S/MIME format, DES, 3 recipients, keyid",
      [ "-encrypt", "-in", $smcont,
	"-stream", "-out", "test.cms", "-keyid",
	catfile($smdir, "smrsa1.pem"),
	catfile($smdir, "smrsa2.pem"),
	catfile($smdir, "smrsa3.pem") ],
      [ "-decrypt", "-recip", catfile($smdir, "smrsa1.pem"),
	"-in", "test.cms", "-out", "smtst.txt" ]
    ],

    [ "enveloped content test streaming PEM format, KEK",
      [ "-encrypt", "-in", $smcont, "-outform", "PEM", "-aes128",
	"-stream", "-out", "test.cms",
	"-secretkey", "000102030405060708090A0B0C0D0E0F",
	"-secretkeyid", "C0FEE0" ],
      [ "-decrypt", "-in", "test.cms", "-out", "smtst.txt", "-inform", "PEM",
	"-secretkey", "000102030405060708090A0B0C0D0E0F",
	"-secretkeyid", "C0FEE0" ]
    ],

    [ "enveloped content test streaming PEM format, KEK, key only",
      [ "-encrypt", "-in", $smcont, "-outform", "PEM", "-aes128",
	"-stream", "-out", "test.cms",
	"-secretkey", "000102030405060708090A0B0C0D0E0F",
	"-secretkeyid", "C0FEE0" ],
      [ "-decrypt", "-in", "test.cms", "-out", "smtst.txt", "-inform", "PEM",
	"-secretkey", "000102030405060708090A0B0C0D0E0F" ]
    ],

    [ "data content test streaming PEM format",
      [ "-data_create", "-in", $smcont, "-outform", "PEM", "-nodetach",
	"-stream", "-out", "test.cms" ],
      [ "-data_out", "-in", "test.cms", "-inform", "PEM", "-out", "smtst.txt" ]
    ],

    [ "encrypted content test streaming PEM format, 128 bit RC2 key",
      [ "-EncryptedData_encrypt", "-in", $smcont, "-outform", "PEM",
	"-rc2", "-secretkey", "000102030405060708090A0B0C0D0E0F",
	"-stream", "-out", "test.cms" ],
      [ "-EncryptedData_decrypt", "-in", "test.cms", "-inform", "PEM",
	"-secretkey", "000102030405060708090A0B0C0D0E0F", "-out", "smtst.txt" ]
    ],

    [ "encrypted content test streaming PEM format, 40 bit RC2 key",
      [ "-EncryptedData_encrypt", "-in", $smcont, "-outform", "PEM",
	"-rc2", "-secretkey", "0001020304",
	"-stream", "-out", "test.cms" ],
      [ "-EncryptedData_decrypt", "-in", "test.cms", "-inform", "PEM",
	"-secretkey", "0001020304", "-out", "smtst.txt" ]
    ],

    [ "encrypted content test streaming PEM format, triple DES key",
      [ "-EncryptedData_encrypt", "-in", $smcont, "-outform", "PEM",
	"-des3", "-secretkey", "000102030405060708090A0B0C0D0E0F1011121314151617",
	"-stream", "-out", "test.cms" ],
      [ "-EncryptedData_decrypt", "-in", "test.cms", "-inform", "PEM",
	"-secretkey", "000102030405060708090A0B0C0D0E0F1011121314151617",
	"-out", "smtst.txt" ]
    ],

    [ "encrypted content test streaming PEM format, 128 bit AES key",
      [ "-EncryptedData_encrypt", "-in", $smcont, "-outform", "PEM",
	"-aes128", "-secretkey", "000102030405060708090A0B0C0D0E0F",
	"-stream", "-out", "test.cms" ],
      [ "-EncryptedData_decrypt", "-in", "test.cms", "-inform", "PEM",
	"-secretkey", "000102030405060708090A0B0C0D0E0F", "-out", "smtst.txt" ]
    ],

);

my @smime_cms_comp_tests = (

    [ "compressed content test streaming PEM format",
      [ "-compress", "-in", $smcont, "-outform", "PEM", "-nodetach",
	"-stream", "-out", "test.cms" ],
      [ "-uncompress", "-in", "test.cms", "-inform", "PEM", "-out", "smtst.txt" ]
    ]

);

my @smime_cms_param_tests = (
    [ "signed content test streaming PEM format, RSA keys, PSS signature",
      [ "-sign", "-in", $smcont, "-outform", "PEM", "-nodetach",
	"-signer", catfile($smdir, "smrsa1.pem"), "-keyopt", "rsa_padding_mode:pss",
	"-out", "test.cms" ],
      [ "-verify", "-in", "test.cms", "-inform", "PEM",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "signed content test streaming PEM format, RSA keys, PSS signature, no attributes",
      [ "-sign", "-in", $smcont, "-outform", "PEM", "-nodetach", "-noattr",
	"-signer", catfile($smdir, "smrsa1.pem"), "-keyopt", "rsa_padding_mode:pss",
	"-out", "test.cms" ],
      [ "-verify", "-in", "test.cms", "-inform", "PEM",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "signed content test streaming PEM format, RSA keys, PSS signature, SHA384 MGF1",
      [ "-sign", "-in", $smcont, "-outform", "PEM", "-nodetach",
	"-signer", catfile($smdir, "smrsa1.pem"), "-keyopt", "rsa_padding_mode:pss",
	"-keyopt", "rsa_mgf1_md:sha384", "-out", "test.cms" ],
      [ "-verify", "-in", "test.cms", "-inform", "PEM",
	"-CAfile", catfile($smdir, "smroot.pem"), "-out", "smtst.txt" ]
    ],

    [ "enveloped content test streaming S/MIME format, DES, OAEP default parameters",
      [ "-encrypt", "-in", $smcont,
	"-stream", "-out", "test.cms",
	"-recip", catfile($smdir, "smrsa1.pem"), "-keyopt", "rsa_padding_mode:oaep" ],
      [ "-decrypt", "-recip", catfile($smdir, "smrsa1.pem"),
	"-in", "test.cms", "-out", "smtst.txt" ]
    ],

    [ "enveloped content test streaming S/MIME format, DES, OAEP SHA256",
      [ "-encrypt", "-in", $smcont,
	"-stream", "-out", "test.cms",
	"-recip", catfile($smdir, "smrsa1.pem"), "-keyopt", "rsa_padding_mode:oaep",
	"-keyopt", "rsa_oaep_md:sha256" ],
      [ "-decrypt", "-recip", catfile($smdir, "smrsa1.pem"),
	"-in", "test.cms", "-out", "smtst.txt" ]
    ],

    [ "enveloped content test streaming S/MIME format, DES, ECDH",
      [ "-encrypt", "-in", $smcont,
	"-stream", "-out", "test.cms",
	"-recip", catfile($smdir, "smec1.pem") ],
      [ "-decrypt", "-recip", catfile($smdir, "smec1.pem"),
	"-in", "test.cms", "-out", "smtst.txt" ]
    ],

    [ "enveloped content test streaming S/MIME format, ECDH, DES, key identifier",
      [ "-encrypt", "-keyid", "-in", $smcont,
	"-stream", "-out", "test.cms",
	"-recip", catfile($smdir, "smec1.pem") ],
      [ "-decrypt", "-recip", catfile($smdir, "smec1.pem"),
	"-in", "test.cms", "-out", "smtst.txt" ]
    ],

    [ "enveloped content test streaming S/MIME format, ECDH, AES128, SHA256 KDF",
      [ "-encrypt", "-in", $smcont,
	"-stream", "-out", "test.cms",
	"-recip", catfile($smdir, "smec1.pem"), "-aes128", "-keyopt", "ecdh_kdf_md:sha256" ],
      [ "-decrypt", "-recip", catfile($smdir, "smec1.pem"),
	"-in", "test.cms", "-out", "smtst.txt" ]
    ],

    [ "enveloped content test streaming S/MIME format, ECDH, K-283, cofactor DH",
      [ "-encrypt", "-in", $smcont,
	"-stream", "-out", "test.cms",
	"-recip", catfile($smdir, "smec2.pem"), "-aes128",
	"-keyopt", "ecdh_kdf_md:sha256", "-keyopt", "ecdh_cofactor_mode:1" ],
      [ "-decrypt", "-recip", catfile($smdir, "smec2.pem"),
	"-in", "test.cms", "-out", "smtst.txt" ]
    ],

    [ "enveloped content test streaming S/MIME format, X9.42 DH",
      [ "-encrypt", "-in", $smcont,
	"-stream", "-out", "test.cms",
	"-recip", catfile($smdir, "smdh.pem"), "-aes128" ],
      [ "-decrypt", "-recip", catfile($smdir, "smdh.pem"),
	"-in", "test.cms", "-out", "smtst.txt" ]
    ]
    );

subtest "CMS => PKCS#7 compatibility tests\n" => sub {
    plan tests => scalar @smime_pkcs7_tests;

    foreach (@smime_pkcs7_tests) {
      SKIP: {
	  my $skip_reason = check_availability($$_[0]);
	  skip $skip_reason, 1 if $skip_reason;

	  ok(run(app(["openssl", "cms", @{$$_[1]}]))
	     && run(app(["openssl", "smime", @{$$_[2]}]))
	     && compare_text($smcont, "smtst.txt") == 0,
	     $$_[0]);
	}
    }
};
subtest "CMS <= PKCS#7 compatibility tests\n" => sub {
    plan tests => scalar @smime_pkcs7_tests;

    foreach (@smime_pkcs7_tests) {
      SKIP: {
	  my $skip_reason = check_availability($$_[0]);
	  skip $skip_reason, 1 if $skip_reason;

	  ok(run(app(["openssl", "smime", @{$$_[1]}]))
	     && run(app(["openssl", "cms", @{$$_[2]}]))
	     && compare_text($smcont, "smtst.txt") == 0,
	     $$_[0]);
	}
    }
};

subtest "CMS <=> CMS consistency tests\n" => sub {
    plan tests => (scalar @smime_pkcs7_tests) + (scalar @smime_cms_tests);

    foreach (@smime_pkcs7_tests) {
      SKIP: {
	  my $skip_reason = check_availability($$_[0]);
	  skip $skip_reason, 1 if $skip_reason;

	  ok(run(app(["openssl", "cms", @{$$_[1]}]))
	     && run(app(["openssl", "cms", @{$$_[2]}]))
	     && compare_text($smcont, "smtst.txt") == 0,
	     $$_[0]);
	}
    }
    foreach (@smime_cms_tests) {
      SKIP: {
	  my $skip_reason = check_availability($$_[0]);
	  skip $skip_reason, 1 if $skip_reason;

	  ok(run(app(["openssl", "cms", @{$$_[1]}]))
	     && run(app(["openssl", "cms", @{$$_[2]}]))
	     && compare_text($smcont, "smtst.txt") == 0,
	     $$_[0]);
	}
    }
};

subtest "CMS <=> CMS consistency tests, modified key parameters\n" => sub {
    plan tests =>
	(scalar @smime_cms_param_tests) + (scalar @smime_cms_comp_tests);

    foreach (@smime_cms_param_tests) {
      SKIP: {
	  my $skip_reason = check_availability($$_[0]);
	  skip $skip_reason, 1 if $skip_reason;

	  ok(run(app(["openssl", "cms", @{$$_[1]}]))
	     && run(app(["openssl", "cms", @{$$_[2]}]))
	     && compare_text($smcont, "smtst.txt") == 0,
	     $$_[0]);
	}
    }

  SKIP: {
      skip("Zlib not supported: compression tests skipped",
	   scalar @smime_cms_comp_tests)
	  if $no_zlib;

      foreach (@smime_cms_comp_tests) {
	SKIP: {
	    my $skip_reason = check_availability($$_[0]);
	    skip $skip_reason, 1 if $skip_reason;

	    ok(run(app(["openssl", "cms", @{$$_[1]}]))
	       && run(app(["openssl", "cms", @{$$_[2]}]))
	       && compare_text($smcont, "smtst.txt") == 0,
	       $$_[0]);
	  }
      }
    }
};

unlink "test.cms";
unlink "test2.cms";
unlink "smtst.txt";

sub check_availability {
    my $tnam = shift;

    return "$tnam: skipped, EC disabled\n"
        if ($no_ec && $tnam =~ /ECDH/);
    return "$tnam: skipped, ECDH disabled\n"
        if ($no_ec && $tnam =~ /ECDH/);
    return "$tnam: skipped, EC2M disabled\n"
        if ($no_ec2m && $tnam =~ /K-283/);
    return "$tnam: skipped, DH disabled\n"
        if ($no_dh && $tnam =~ /X9\.42/);
    return "$tnam: skipped, RC2 disabled\n"
        if ($no_rc2 && $tnam =~ /RC2/);
    return "$tnam: skipped, DES disabled\n"
        if ($no_des && $tnam =~ /DES/);
    return "$tnam: skipped, DSA disabled\n"
        if ($no_dsa && $tnam =~ / DSA/);

    return "";
}
