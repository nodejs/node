#! /usr/bin/env perl
# Copyright 2015-2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use POSIX;
use File::Spec::Functions qw/catfile/;
use File::Compare qw/compare_text compare/;
use OpenSSL::Test qw/:DEFAULT srctop_dir srctop_file bldtop_dir bldtop_file with data_file/;

use OpenSSL::Test::Utils;

BEGIN {
    setup("test_cms");
}

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);
my $old_fips = 0;

plan skip_all => "CMS is not supported by this OpenSSL build"
    if disabled("cms");

my $provpath = bldtop_dir("providers");

# Some tests require legacy algorithms to be included.
my @legacyprov = ("-provider-path", $provpath,
                  "-provider", "default",
                  "-provider", "legacy" );
my @defaultprov = ("-provider-path", $provpath,
                   "-provider", "default");

my @config = ( );
my $provname = 'default';
my $dsaallow = '1';
my $no_pqc = 0;

my $datadir = srctop_dir("test", "recipes", "80-test_cms_data");
my $smdir    = srctop_dir("test", "smime-certs");
my $smcont   = srctop_file("test", "smcont.txt");
my $smcont_zero = srctop_file("test", "smcont_zero.txt");
my ($no_des, $no_dh, $no_dsa, $no_ec, $no_ec2m, $no_rc2, $no_zlib)
    = disabled qw/des dh dsa ec ec2m rc2 zlib/;

$no_rc2 = 1 if disabled("legacy");

plan tests => 30;

ok(run(test(["pkcs7_test"])), "test pkcs7");

unless ($no_fips) {
    my $provconf = srctop_file("test", "fips-and-base.cnf");
    @config = ( "-config", $provconf );
    $provname = 'fips';

    run(test(["fips_version_test", "-config", $provconf, "<3.4.0"]),
        capture => 1, statusvar => \$dsaallow);
    $no_dsa = 1 if $dsaallow == '0';
    $old_fips = 1 if $dsaallow != '0';
    run(test(["fips_version_test", "-config", $provconf, "<3.5.0"]),
        capture => 1, statusvar => \$no_pqc);
}

$ENV{OPENSSL_TEST_LIBCTX} = "1";
my @prov = ("-provider-path", $provpath,
            @config,
            "-provider", $provname);

my $smrsa1024 = catfile($smdir, "smrsa1024.pem");
my $smrsa1 = catfile($smdir, "smrsa1.pem");
my $smroot = catfile($smdir, "smroot.pem");

my @smime_pkcs7_tests = (

    [ "signed content DER format, RSA key",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "DER", "-nodetach",
        "-certfile", $smroot, "-signer", $smrsa1, "-out", "{output}.cms" ],
      [ "{cmd2}",  @prov, "-verify", "-in", "{output}.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed detached content DER format, RSA key",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "DER",
        "-signer", $smrsa1, "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt",
        "-content", $smcont ],
      \&final_compare
    ],

    [ "signed content test streaming BER format, RSA",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "DER", "-nodetach",
        "-stream",
        "-signer", $smrsa1, "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content DER format, DSA key",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "DER", "-nodetach",
        "-signer", catfile($smdir, "smdsa1.pem"), "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed detached content DER format, DSA key",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "DER",
        "-signer", catfile($smdir, "smdsa1.pem"), "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt",
        "-content", $smcont ],
      \&final_compare
    ],

    [ "signed detached content DER format, add RSA signer (with DSA existing)",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "DER",
        "-signer", catfile($smdir, "smdsa1.pem"), "-out", "{output}.cms" ],
      [ "{cmd1}", @prov, "-resign", "-in", "{output}.cms", "-inform", "DER", "-outform", "DER",
        "-signer", $smrsa1, "-out", "{output}2.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}2.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt",
        "-content", $smcont ],
      \&final_compare
    ],

    [ "signed content test streaming BER format, DSA key",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "DER",
        "-nodetach", "-stream",
        "-signer", catfile($smdir, "smdsa1.pem"), "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content test streaming BER format, 2 DSA and 2 RSA keys",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "DER",
        "-nodetach", "-stream",
        "-signer", $smrsa1,
        "-signer", catfile($smdir, "smrsa2.pem"),
        "-signer", catfile($smdir, "smdsa1.pem"),
        "-signer", catfile($smdir, "smdsa2.pem"),
        "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content test streaming BER format, 2 DSA and 2 RSA keys, no attributes",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "DER",
        "-noattr", "-nodetach", "-stream",
        "-signer", $smrsa1,
        "-signer", catfile($smdir, "smrsa2.pem"),
        "-signer", catfile($smdir, "smdsa1.pem"),
        "-signer", catfile($smdir, "smdsa2.pem"),
        "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content S/MIME format, RSA key SHA1",
      [ "{cmd1}", @defaultprov, "-sign", "-in", $smcont, "-md", "sha1",
        "-certfile", $smroot,
        "-signer", $smrsa1, "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed zero-length content S/MIME format, RSA key SHA1",
      [ "{cmd1}", @defaultprov, "-sign", "-in", $smcont_zero, "-md", "sha1",
        "-certfile", $smroot, "-signer", $smrsa1, "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&zero_compare
    ],

    [ "signed content test streaming S/MIME format, 2 DSA and 2 RSA keys",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-nodetach",
        "-signer", $smrsa1,
        "-signer", catfile($smdir, "smrsa2.pem"),
        "-signer", catfile($smdir, "smdsa1.pem"),
        "-signer", catfile($smdir, "smdsa2.pem"),
        "-stream", "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content test streaming multipart S/MIME format, 2 DSA and 2 RSA keys",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont,
        "-signer", $smrsa1,
        "-signer", catfile($smdir, "smrsa2.pem"),
        "-signer", catfile($smdir, "smdsa1.pem"),
        "-signer", catfile($smdir, "smdsa2.pem"),
        "-stream", "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "enveloped content test streaming S/MIME format, DES, 3 recipients",
      [ "{cmd1}", @defaultprov, "-encrypt", "-in", $smcont,
        "-stream", "-out", "{output}.cms",
        $smrsa1,
        catfile($smdir, "smrsa2.pem"),
        catfile($smdir, "smrsa3.pem") ],
      [ "{cmd2}", @defaultprov, "-decrypt", "-recip", $smrsa1,
        "-in", "{output}.cms", "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "enveloped content test streaming S/MIME format, DES, 3 recipients, 3rd used",
      [ "{cmd1}", @defaultprov, "-encrypt", "-in", $smcont,
        "-stream", "-out", "{output}.cms",
        $smrsa1,
        catfile($smdir, "smrsa2.pem"),
        catfile($smdir, "smrsa3.pem") ],
      [ "{cmd2}", @defaultprov, "-decrypt", "-recip", catfile($smdir, "smrsa3.pem"),
        "-in", "{output}.cms", "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "enveloped content test streaming S/MIME format, DES, 3 recipients, cert and key files used",
      [ "{cmd1}", @defaultprov, "-encrypt", "-in", $smcont,
        "-stream", "-out", "{output}.cms",
        $smrsa1,
        catfile($smdir, "smrsa2.pem"),
        catfile($smdir, "smrsa3-cert.pem") ],
      [ "{cmd2}", @defaultprov, "-decrypt",
	"-recip", catfile($smdir, "smrsa3-cert.pem"),
	"-inkey", catfile($smdir, "smrsa3-key.pem"),
        "-in", "{output}.cms", "-out", "{output}.txt" ],
      \&final_compare
    ],

);

if ($no_fips || $old_fips) {
    push(@smime_pkcs7_tests,
         [ "enveloped content test streaming S/MIME format, AES-256 cipher, 3 recipients",
           [ "{cmd1}", @prov, "-encrypt", "-in", $smcont,
             "-aes256", "-stream", "-out", "{output}.cms",
             $smrsa1,
             catfile($smdir, "smrsa2.pem"),
             catfile($smdir, "smrsa3.pem") ],
           [ "{cmd2}", @prov, "-decrypt", "-recip", $smrsa1,
             "-in", "{output}.cms", "-out", "{output}.txt" ],
           \&final_compare
         ]
    );
}

my @smime_cms_tests = (

    [ "signed content test streaming BER format, 2 DSA and 2 RSA keys, keyid",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "DER",
        "-nodetach", "-keyid",
        "-signer", $smrsa1,
        "-signer", catfile($smdir, "smrsa2.pem"),
        "-signer", catfile($smdir, "smdsa1.pem"),
        "-signer", catfile($smdir, "smdsa2.pem"),
        "-stream", "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content test streaming PEM format, 2 DSA and 2 RSA keys",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "PEM", "-nodetach",
        "-signer", $smrsa1,
        "-signer", catfile($smdir, "smrsa2.pem"),
        "-signer", catfile($smdir, "smdsa1.pem"),
        "-signer", catfile($smdir, "smdsa2.pem"),
        "-stream", "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "PEM",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content MIME format, RSA key, signed receipt request",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-nodetach",
        "-signer", $smrsa1,
        "-receipt_request_to", "test\@openssl.org", "-receipt_request_all",
        "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed receipt MIME format, RSA key",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-nodetach",
        "-signer", $smrsa1,
        "-receipt_request_to", "test\@openssl.org", "-receipt_request_all",
        "-out", "{output}.cms" ],
      [ "{cmd1}", @prov, "-sign_receipt", "-in", "{output}.cms",
        "-signer", catfile($smdir, "smrsa2.pem"), "-out", "{output}2.cms" ],
      [ "{cmd2}", @prov, "-verify_receipt", "{output}2.cms", "-in", "{output}.cms",
        "-CAfile", $smroot ]
    ],

    [ "enveloped content test streaming S/MIME format, DES, 3 recipients, keyid",
      [ "{cmd1}", @defaultprov, "-encrypt", "-in", $smcont,
        "-stream", "-out", "{output}.cms", "-keyid",
        $smrsa1,
        catfile($smdir, "smrsa2.pem"),
        catfile($smdir, "smrsa3.pem") ],
      [ "{cmd2}", @defaultprov, "-decrypt", "-recip", $smrsa1,
        "-in", "{output}.cms", "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "enveloped content test streaming PEM format, AES-256-CBC cipher, KEK",
      [ "{cmd1}", @prov, "-encrypt", "-in", $smcont, "-outform", "PEM", "-aes128",
        "-stream", "-out", "{output}.cms",
        "-secretkey", "000102030405060708090A0B0C0D0E0F",
        "-secretkeyid", "C0FEE0" ],
      [ "{cmd2}", @prov, "-decrypt", "-in", "{output}.cms", "-out", "{output}.txt",
        "-inform", "PEM",
        "-secretkey", "000102030405060708090A0B0C0D0E0F",
        "-secretkeyid", "C0FEE0" ],
      \&final_compare
    ],

    [ "enveloped content test streaming PEM format, AES-256-GCM cipher, KEK",
      [ "{cmd1}", @prov, "-encrypt", "-in", $smcont, "-outform", "PEM", "-aes-128-gcm",
        "-stream", "-out", "{output}.cms",
        "-secretkey", "000102030405060708090A0B0C0D0E0F",
        "-secretkeyid", "C0FEE0" ],
      [ "{cmd2}", "-decrypt", "-in", "{output}.cms", "-out", "{output}.txt",
        "-inform", "PEM",
        "-secretkey", "000102030405060708090A0B0C0D0E0F",
        "-secretkeyid", "C0FEE0" ],
      \&final_compare
    ],

    [ "enveloped content test streaming PEM format, KEK, key only",
      [ "{cmd1}", @prov, "-encrypt", "-in", $smcont, "-outform", "PEM", "-aes128",
        "-stream", "-out", "{output}.cms",
        "-secretkey", "000102030405060708090A0B0C0D0E0F",
        "-secretkeyid", "C0FEE0" ],
      [ "{cmd2}", @prov, "-decrypt", "-in", "{output}.cms", "-out", "{output}.txt",
        "-inform", "PEM",
        "-secretkey", "000102030405060708090A0B0C0D0E0F" ],
      \&final_compare
    ],

    [ "enveloped content test streaming PEM format, AES-128-CBC cipher, password",
      [ "{cmd1}", @prov, "-encrypt", "-in", $smcont, "-outform", "PEM", "-aes128",
        "-stream", "-out", "{output}.cms",
        "-pwri_password", "test" ],
      [ "{cmd2}", @prov, "-decrypt", "-in", "{output}.cms", "-out", "{output}.txt",
        "-inform", "PEM",
        "-pwri_password", "test" ],
      \&final_compare
    ],

    [ "data content test streaming PEM format",
      [ "{cmd1}", @prov, "-data_create", "-in", $smcont, "-outform", "PEM",
        "-nodetach", "-stream", "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-data_out", "-in", "{output}.cms", "-inform", "PEM",
        "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "encrypted content test streaming PEM format, 128 bit RC2 key",
      [ "{cmd1}", @legacyprov, "-EncryptedData_encrypt",
        "-in", $smcont, "-outform", "PEM",
        "-rc2", "-secretkey", "000102030405060708090A0B0C0D0E0F",
        "-stream", "-out", "{output}.cms" ],
      [ "{cmd2}", @legacyprov, "-EncryptedData_decrypt", "-in", "{output}.cms",
        "-inform", "PEM",
        "-secretkey", "000102030405060708090A0B0C0D0E0F",
        "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "encrypted content test streaming PEM format, 40 bit RC2 key",
      [ "{cmd1}", @legacyprov, "-EncryptedData_encrypt",
        "-in", $smcont, "-outform", "PEM",
        "-rc2", "-secretkey", "0001020304",
        "-stream", "-out", "{output}.cms" ],
      [ "{cmd2}", @legacyprov, "-EncryptedData_decrypt", "-in", "{output}.cms",
        "-inform", "PEM",
        "-secretkey", "0001020304", "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "encrypted content test streaming PEM format, triple DES key",
      [ "{cmd1}", @defaultprov, "-EncryptedData_encrypt", "-in", $smcont, "-outform", "PEM",
        "-des3", "-secretkey", "000102030405060708090A0B0C0D0E0F1011121314151617",
        "-stream", "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-EncryptedData_decrypt", "-in", "{output}.cms",
        "-inform", "PEM",
        "-secretkey", "000102030405060708090A0B0C0D0E0F1011121314151617",
        "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "encrypted content test streaming PEM format, 128 bit AES key",
      [ "{cmd1}", @prov, "-EncryptedData_encrypt", "-in", $smcont, "-outform", "PEM",
        "-aes128", "-secretkey", "000102030405060708090A0B0C0D0E0F",
        "-stream", "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-EncryptedData_decrypt", "-in", "{output}.cms",
        "-inform", "PEM",
        "-secretkey", "000102030405060708090A0B0C0D0E0F",
        "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "encrypted content test streaming PEM format -noout, 128 bit AES key",
      [ "{cmd1}", @prov, "-EncryptedData_encrypt", "-in", $smcont, "-outform", "PEM",
	"-aes128", "-secretkey", "000102030405060708090A0B0C0D0E0F",
	"-stream", "-noout" ],
      [ "{cmd2}", @prov, "-help" ]
    ],
);

my @smime_cms_cades_tests = (

    [ "signed content DER format, RSA key, CAdES-BES compatible",
      [ "{cmd1}", @prov, "-sign", "-cades", "-in", $smcont, "-outform", "DER",
         "-nodetach",
        "-certfile", $smroot, "-signer", $smrsa1, "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-cades", "-in", "{output}.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content DER format, RSA key, SHA256 md, CAdES-BES compatible",
      [ "{cmd1}", @prov, "-sign", "-cades", "-md", "sha256", "-in", $smcont, "-outform",
        "DER", "-nodetach", "-certfile", $smroot,
        "-signer", $smrsa1, "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-cades", "-in", "{output}.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content DER format, RSA key, SHA512 md, CAdES-BES compatible",
      [ "{cmd1}", @prov, "-sign", "-cades", "-md", "sha512", "-in", $smcont, "-outform",
        "DER", "-nodetach", "-certfile", $smroot,
        "-signer", $smrsa1, "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-cades", "-in", "{output}.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content DER format, RSA key, SHA256 md, CAdES-BES compatible",
      [ "{cmd1}", @prov, "-sign", "-cades", "-binary",  "-nodetach", "-nosmimecap", "-md", "sha256",
        "-in", $smcont, "-outform", "DER", 
        "-certfile", $smroot, "-signer", $smrsa1,
        "-outform", "DER", "-out", "{output}.cms"  ],
      [ "{cmd2}", @prov, "-verify", "-cades", "-in", "{output}.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "resigned content DER format, RSA key, SHA256 md, CAdES-BES compatible",
      [ "{cmd1}", @prov, "-sign", "-cades", "-binary",  "-nodetach", "-nosmimecap", "-md", "sha256",
        "-in", $smcont, "-outform", "DER", 
        "-certfile", $smroot, "-signer", $smrsa1,
        "-outform", "DER", "-out", "{output}.cms"  ],
      [ "{cmd1}", @prov, "-resign", "-cades", "-binary", "-nodetach", "-nosmimecap", "-md", "sha256",
        "-inform", "DER", "-in", "{output}.cms",
        "-certfile", $smroot, "-signer", catfile($smdir, "smrsa2.pem"),
        "-outform", "DER", "-out", "{output}2.cms" ],

      [ "{cmd2}", @prov, "-verify", "-cades", "-in", "{output}2.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],
);

my @smime_cms_cades_ko_tests = (
    [ "sign content DER format, RSA key, not CAdES-BES compatible",
      [ @prov, "-sign", "-in", $smcont, "-outform", "DER", "-nodetach",
        "-certfile", $smroot, "-signer", $smrsa1, "-out", "cades-ko.cms" ],
      "fail to verify token since requiring CAdES-BES compatibility",
      [ @prov, "-verify", "-cades", "-in", "cades-ko.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "cades-ko.txt" ],
      \&final_compare
    ]
);

# cades options test - check that some combinations are rejected
my @smime_cms_cades_invalid_option_tests = (
    [
        [ "-cades", "-noattr" ],
    ],[
        [ "-verify", "-cades", "-noattr" ],
    ],[
        [ "-verify", "-cades", "-noverify" ],
    ],
);

my @smime_cms_comp_tests = (

    [ "compressed content test streaming PEM format",
      [ "{cmd1}", @prov, "-compress", "-in", $smcont, "-outform", "PEM", "-nodetach",
        "-stream", "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-uncompress", "-in", "{output}.cms", "-inform", "PEM",
        "-out", "{output}.txt" ],
      \&final_compare
    ]

);

my @smime_cms_param_tests = (
    [ "signed content test streaming PEM format, RSA keys, PSS signature",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "PEM", "-nodetach",
        "-signer", $smrsa1,
        "-keyopt", "rsa_padding_mode:pss",
        "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "PEM",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content test streaming PEM format, RSA keys, PSS signature, saltlen=max",
      [ "{cmd1}", @defaultprov, "-sign", "-in", $smcont, "-outform", "PEM", "-nodetach",
        "-signer", $smrsa1,
        "-keyopt", "rsa_padding_mode:pss", "-keyopt", "rsa_pss_saltlen:max",
        "-out", "{output}.cms" ],
      sub { my %opts = @_; rsapssSaltlen("$opts{output}.cms") == 222; },
      [ "{cmd2}", @defaultprov, "-verify", "-in", "{output}.cms", "-inform", "PEM",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content test streaming PEM format, RSA keys, PSS signature, no attributes",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "PEM", "-nodetach",
        "-noattr", "-signer", $smrsa1,
        "-keyopt", "rsa_padding_mode:pss",
        "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "PEM",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content test streaming PEM format, RSA keys, PSS signature, SHA384 MGF1",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "PEM", "-nodetach",
        "-signer", $smrsa1,
        "-keyopt", "rsa_padding_mode:pss", "-keyopt", "rsa_mgf1_md:sha384",
        "-out", "{output}.cms" ],
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "PEM",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content test streaming PEM format, RSA keys, PSS signature, saltlen=16",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "PEM", "-nodetach",
        "-signer", $smrsa1, "-md", "sha256",
        "-keyopt", "rsa_padding_mode:pss", "-keyopt", "rsa_pss_saltlen:16",
        "-out", "{output}.cms" ],
      sub { my %opts = @_; rsapssSaltlen("$opts{output}.cms") == 16; },
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "PEM",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content test streaming PEM format, RSA keys, PSS signature, saltlen=digest",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "PEM", "-nodetach",
        "-signer", $smrsa1, "-md", "sha256",
        "-keyopt", "rsa_padding_mode:pss", "-keyopt", "rsa_pss_saltlen:digest",
        "-out", "{output}.cms" ],
      # digest is SHA-256, which produces 32 bytes of output
      sub { my %opts = @_; rsapssSaltlen("$opts{output}.cms") == 32; },
      [ "{cmd2}", @prov, "-verify", "-in", "{output}.cms", "-inform", "PEM",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "enveloped content test streaming S/MIME format, DES, OAEP default parameters",
      [ "{cmd1}", @defaultprov, "-encrypt", "-in", $smcont,
        "-stream", "-out", "{output}.cms",
        "-recip", $smrsa1,
        "-keyopt", "rsa_padding_mode:oaep" ],
      [ "{cmd2}", @defaultprov, "-decrypt", "-recip", $smrsa1,
        "-in", "{output}.cms", "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "enveloped content test streaming S/MIME format, DES, OAEP SHA256",
      [ "{cmd1}", @defaultprov, "-encrypt", "-in", $smcont,
        "-stream", "-out", "{output}.cms",
        "-recip", $smrsa1,
        "-keyopt", "rsa_padding_mode:oaep",
        "-keyopt", "rsa_oaep_md:sha256" ],
      [ "{cmd2}", @defaultprov, "-decrypt", "-recip", $smrsa1,
        "-in", "{output}.cms", "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "enveloped content test streaming S/MIME format, DES, ECDH",
      [ "{cmd1}", @defaultprov, "-encrypt", "-in", $smcont,
        "-stream", "-out", "{output}.cms",
        "-recip", catfile($smdir, "smec1.pem") ],
      [ "{cmd2}", @defaultprov, "-decrypt", "-recip", catfile($smdir, "smec1.pem"),
        "-in", "{output}.cms", "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "enveloped content test streaming S/MIME format, DES, ECDH, 2 recipients, key only used",
      [ "{cmd1}", @defaultprov, "-encrypt", "-in", $smcont,
        "-stream", "-out", "{output}.cms",
        catfile($smdir, "smec1.pem"),
        catfile($smdir, "smec3.pem") ],
      [ "{cmd2}", @defaultprov, "-decrypt", "-inkey", catfile($smdir, "smec3.pem"),
        "-in", "{output}.cms", "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "enveloped content test streaming S/MIME format, ECDH, DES, key identifier",
      [ "{cmd1}", @defaultprov, "-encrypt", "-keyid", "-in", $smcont,
        "-stream", "-out", "{output}.cms",
        "-recip", catfile($smdir, "smec1.pem") ],
      [ "{cmd2}", @defaultprov, "-decrypt", "-recip", catfile($smdir, "smec1.pem"),
        "-in", "{output}.cms", "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "enveloped content test streaming S/MIME format, ECDH, AES-128-CBC, SHA256 KDF",
      [ "{cmd1}", @defaultprov, "-encrypt", "-in", $smcont,
        "-stream", "-out", "{output}.cms",
        "-recip", catfile($smdir, "smec1.pem"), "-aes128",
        "-keyopt", "ecdh_kdf_md:sha256" ],
      sub { my %opts = @_; smimeType_matches("$opts{output}.cms", "enveloped-data"); },
      [ "{cmd2}", @defaultprov, "-decrypt", "-recip", catfile($smdir, "smec1.pem"),
        "-in", "{output}.cms", "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "enveloped content test streaming S/MIME format, ECDH, AES-128-GCM cipher, SHA256 KDF",
      [ "{cmd1}", @defaultprov, "-encrypt", "-in", $smcont,
        "-stream", "-out", "{output}.cms",
        "-recip", catfile($smdir, "smec1.pem"), "-aes-128-gcm", "-keyopt", "ecdh_kdf_md:sha256" ],
      sub { my %opts = @_; smimeType_matches("$opts{output}.cms", "authEnveloped-data"); },
      [ "{cmd2}", "-decrypt", "-recip", catfile($smdir, "smec1.pem"),
        "-in", "{output}.cms", "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "enveloped content test streaming S/MIME format, ECDH, K-283, cofactor DH",
      [ "{cmd1}", @defaultprov, "-encrypt", "-in", $smcont,
        "-stream", "-out", "{output}.cms",
        "-recip", catfile($smdir, "smec2.pem"), "-aes128",
        "-keyopt", "ecdh_kdf_md:sha256", "-keyopt", "ecdh_cofactor_mode:1" ],
      [ "{cmd2}", @defaultprov, "-decrypt", "-recip", catfile($smdir, "smec2.pem"),
        "-in", "{output}.cms", "-out", "{output}.txt" ],
      \&final_compare
    ]
);

if ($no_fips || $old_fips) {
    # Only SHA1 supported in dh_cms_encrypt()
    push(@smime_cms_param_tests,

	 [ "enveloped content test streaming S/MIME format, X9.42 DH",
	   [ "{cmd1}", @prov, "-encrypt", "-in", $smcont,
	     "-stream", "-out", "{output}.cms",
	     "-recip", catfile($smdir, "smdh.pem"), "-aes128" ],
	   [ "{cmd2}", @prov, "-decrypt", "-recip", catfile($smdir, "smdh.pem"),
	     "-in", "{output}.cms", "-out", "{output}.txt" ],
	   \&final_compare
	 ]
    );
}

my @smime_cms_param_tests_autodigestmax = (
    [ "signed content test streaming PEM format, RSA keys, PSS signature, saltlen=auto-digestmax, digestsize < maximum salt length",
      [ "{cmd1}", @prov, "-sign", "-in", $smcont, "-outform", "PEM", "-nodetach",
        "-signer", $smrsa1, "-md", "sha256",
        "-keyopt", "rsa_padding_mode:pss", "-keyopt", "rsa_pss_saltlen:auto-digestmax",
        "-out", "{output}.cms" ],
      # digest is SHA-256, which produces 32, bytes of output
      sub { my %opts = @_; rsapssSaltlen("$opts{output}.cms") == 32; },
      [ "{cmd2}", @defaultprov, "-verify", "-in", "{output}.cms", "-inform", "PEM",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ],

    [ "signed content test streaming PEM format, RSA keys, PSS signature, saltlen=auto-digestmax, digestsize > maximum salt length",
      [ "{cmd1}", @defaultprov, "-sign", "-in", $smcont, "-outform", "PEM", "-nodetach",
        "-signer", $smrsa1024, "-md", "sha512",
        "-keyopt", "rsa_padding_mode:pss", "-keyopt", "rsa_pss_saltlen:auto-digestmax",
        "-out", "{output}.cms" ],
      # digest is SHA-512, which produces 64, bytes of output, but an RSA-PSS
      # signature with a 1024 bit RSA key can only accommodate 62
      sub { my %opts = @_; rsapssSaltlen("$opts{output}.cms") == 62; },
      [ "{cmd2}", @defaultprov, "-verify", "-in", "{output}.cms", "-inform", "PEM",
        "-CAfile", $smroot, "-out", "{output}.txt" ],
      \&final_compare
    ]
);


my @contenttype_cms_test = (
    [ "signed content test - check that content type is added to additional signerinfo, RSA keys",
      [ "{cmd1}", @prov, "-sign", "-binary", "-nodetach", "-stream", "-in", $smcont,
        "-outform", "DER", "-signer", $smrsa1, "-md", "SHA256",
        "-out", "{output}.cms" ],
      [ "{cmd1}", @prov, "-resign", "-binary", "-nodetach", "-in", "{output}.cms",
        "-inform", "DER", "-outform", "DER",
        "-signer", catfile($smdir, "smrsa2.pem"), "-md", "SHA256",
        "-out", "{output}2.cms" ],
      sub { my %opts = @_; contentType_matches("$opts{output}2.cms") == 2; },
      [ "{cmd2}", @prov, "-verify", "-in", "{output}2.cms", "-inform", "DER",
        "-CAfile", $smroot, "-out", "{output}.txt" ]
    ],
);

my @incorrect_attribute_cms_test = (
    "bad_signtime_attr.cms",
    "no_ct_attr.cms",
    "no_md_attr.cms",
    "ct_multiple_attr.cms"
);

# Runs a standard loop on the input array
sub runner_loop {
    my %opts = ( @_ );
    my $cnt1 = 0;

    foreach (@{$opts{tests}}) {
        $cnt1++;
        $opts{output} = "$opts{prefix}-$cnt1";
      SKIP: {
          my $skip_reason = check_availability($$_[0]);
          skip $skip_reason, 1 if $skip_reason;
          my $ok = 1;
          1 while unlink "$opts{output}.txt";

          foreach (@$_[1..$#$_]) {
              if (ref $_ eq 'CODE') {
                  $ok &&= $_->(%opts);
              } else {
                  my @cmd = map {
                      my $x = $_;
                      while ($x =~ /\{([^\}]+)\}/) {
                          $x = $`.$opts{$1}.$' if exists $opts{$1};
                      }
                      $x;
                  } @$_;

                  diag "CMD: openssl ", join(" ", @cmd);
                  $ok &&= run(app(["openssl", @cmd]));
                  $opts{input} = $opts{output};
              }
          }

          ok($ok, $$_[0]);
        }
    }
}

sub final_compare {
    my %opts = @_;

    diag "Comparing $smcont with $opts{output}.txt";
    return compare_text($smcont, "$opts{output}.txt") == 0;
}

sub zero_compare {
    my %opts = @_;

    diag "Checking for zero-length file";
    return (-e "$opts{output}.txt" && -z "$opts{output}.txt");
}

subtest "CMS => PKCS#7 compatibility tests\n" => sub {
    plan tests => scalar @smime_pkcs7_tests;

    runner_loop(prefix => 'cms2pkcs7', cmd1 => 'cms', cmd2 => 'smime',
                tests => [ @smime_pkcs7_tests ]);
};
subtest "CMS <= PKCS#7 compatibility tests\n" => sub {
    plan tests => scalar @smime_pkcs7_tests;

    runner_loop(prefix => 'pkcs72cms', cmd1 => 'smime', cmd2 => 'cms',
                tests => [ @smime_pkcs7_tests ]);
};

subtest "CMS <=> CMS consistency tests\n" => sub {
    plan tests => (scalar @smime_pkcs7_tests) + (scalar @smime_cms_tests);

    runner_loop(prefix => 'cms2cms-1', cmd1 => 'cms', cmd2 => 'cms',
                tests => [ @smime_pkcs7_tests ]);
    runner_loop(prefix => 'cms2cms-2', cmd1 => 'cms', cmd2 => 'cms',
                tests => [ @smime_cms_tests ]);
};

subtest "CMS <=> CMS consistency tests, modified key parameters\n" => sub {
    plan tests =>
        (scalar @smime_cms_param_tests) + (scalar @smime_cms_comp_tests) +
        (scalar @smime_cms_param_tests_autodigestmax) + 1;

    ok(run(app(["openssl", "cms", @prov,
                "-sign", "-in", $smcont,
                "-outform", "PEM",
                "-nodetach",
                "-signer", $smrsa1,
                "-keyopt", "rsa_padding_mode:pss",
                "-keyopt", "rsa_pss_saltlen:auto-digestmax",
                "-out", "digestmaxtest.cms"])));
    # Providers that do not support rsa_pss_saltlen:auto-digestmax will parse
    # it as 0
    my $no_autodigestmax = rsapssSaltlen("digestmaxtest.cms") == 0;
    1 while unlink "digestmaxtest.cms";

    runner_loop(prefix => 'cms2cms-mod', cmd1 => 'cms', cmd2 => 'cms',
                tests => [ @smime_cms_param_tests ]);
  SKIP: {
      skip("Zlib not supported: compression tests skipped",
           scalar @smime_cms_comp_tests)
          if $no_zlib;

      runner_loop(prefix => 'cms2cms-comp', cmd1 => 'cms', cmd2 => 'cms',
                  tests => [ @smime_cms_comp_tests ]);
    }

  SKIP: {
    skip("rsa_pss_saltlen:auto-digestmax not supported",
         scalar @smime_cms_param_tests_autodigestmax)
       if $no_autodigestmax;

       runner_loop(prefix => 'cms2cms-comp', 'cmd1' => 'cms', cmd2 => 'cms',
                   tests => [ @smime_cms_param_tests_autodigestmax ]);
  }
};

# Returns the number of matches of a Content Type Attribute in a binary file.
sub contentType_matches {
  # Read in a binary file
  my ($in) = @_;
  open (HEX_IN, "$in") or die("open failed for $in : $!");
  binmode(HEX_IN);
  local $/;
  my $str = <HEX_IN>;

  # Find ASN1 data for a Content Type Attribute (with a OID of PKCS7 data)
  my @c = $str =~ /\x30\x18\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x03\x31\x0B\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x07\x01/gs;

  close(HEX_IN);
  return scalar(@c);
}

# Returns 1 if the smime-type matches the passed parameter, otherwise 0.
sub smimeType_matches {
  my ($in, $expected_smime_type) = @_;

  # Read the text file
  open(my $fh, '<', $in) or die("open failed for $in : $!");
  local $/;
  my $content = <$fh>;
  close($fh);

  # Extract the Content-Type line with the smime-type attribute
  if ($content =~ /Content-Type:\s*application\/pkcs7-mime.*smime-type=([^\s;]+)/) {
    my $smime_type = $1;

    # Compare the extracted smime-type with the expected value
    return ($smime_type eq $expected_smime_type) ? 1 : 0;
  }

  # If no smime-type is found, return 0
  return 0;
}

sub rsapssSaltlen {
  my ($in) = @_;
  my $exit = 0;

  my @asn1parse = run(app(["openssl", "asn1parse", "-in", $in, "-dump"]),
                      capture => 1,
                      statusvar => $exit);
  return -1 if $exit != 0;

  my $pssparam_offset = -1;
  while ($_ = shift @asn1parse) {
    chomp;
    next unless /:rsassaPss/;
    # This line contains :rsassaPss, the next line contains a raw dump of the
    # RSA_PSS_PARAMS sequence; obtain its offset
    $_ = shift @asn1parse;
    if (/^\s*(\d+):/) {
      $pssparam_offset = int($1);
    }
  }

  if ($pssparam_offset == -1) {
    note "Failed to determine RSA_PSS_PARAM offset in CMS. " +
         "Was the file correctly signed with RSASSA-PSS?";
    return -1;
  }

  my @pssparam = run(app(["openssl", "asn1parse", "-in", $in,
                          "-strparse", $pssparam_offset]),
                     capture => 1,
                     statusvar => $exit);
  return -1 if $exit != 0;

  my $saltlen = -1;
  # Can't use asn1parse -item RSA_PSS_PARAMS here, because that's deprecated.
  # This assumes the salt length is the last field, which may possibly be
  # incorrect if there is a non-standard trailer field, but there almost never
  # is in PSS.
  if ($pssparam[-1] =~ /prim:\s+INTEGER\s+:([A-Fa-f0-9]+)/) {
    $saltlen = hex($1);
  }

  if ($saltlen == -1) {
    note "Failed to determine salt length from RSA_PSS_PARAM struct. " +
         "Was the file correctly signed with RSASSA-PSS?";
    return -1;
  }

  return $saltlen;
}

subtest "CMS Check the content type attribute is added for additional signers\n" => sub {
    plan tests => (scalar @contenttype_cms_test);

    runner_loop(prefix => 'cms2cms-added', cmd1 => 'cms', cmd2 => 'cms',
                tests => [ @contenttype_cms_test ]);
};

subtest "CMS Check that bad attributes fail when verifying signers\n" => sub {
    plan tests =>
        (scalar @incorrect_attribute_cms_test);

    my $cnt = 0;
    foreach my $name (@incorrect_attribute_cms_test) {
        my $out = "incorrect-$cnt.txt";

        ok(!run(app(["openssl", "cms", @prov, "-verify", "-in",
                     catfile($datadir, $name), "-inform", "DER", "-CAfile",
                     $smroot, "-out", $out ])),
            $name);
    }
};

subtest "CMS Check that bad encryption algorithm fails\n" => sub {
    plan tests => 1;

    SKIP: {
        skip "DES or Legacy isn't supported in this build", 1
            if disabled("des") || disabled("legacy");

        my $out = "smtst.txt";

        ok(!run(app(["openssl", "cms", @legacyprov, "-encrypt",
                    "-in", $smcont,
                    "-stream", "-recip", $smrsa1,
                    "-des-ede3",
                    "-out", $out ])),
           "Decrypt message from OpenSSL 1.1.1");
    }
};

subtest "CMS Decrypt message encrypted with OpenSSL 1.1.1\n" => sub {
    plan tests => 1;

    SKIP: {
        skip "EC or DES isn't supported in this build", 1
            if disabled("ec") || disabled("des");

        my $out = "smtst.txt";

        ok(run(app(["openssl", "cms", @defaultprov, "-decrypt",
                    "-inkey", catfile($smdir, "smec3.pem"),
                    "-in", catfile($datadir, "ciphertext_from_1_1_1.cms"),
                    "-out", $out ]))
           && compare_text($smcont, $out) == 0,
           "Decrypt message from OpenSSL 1.1.1");
    }
};

subtest "CAdES <=> CAdES consistency tests\n" => sub {
    plan tests => (scalar @smime_cms_cades_tests);

    runner_loop(prefix => 'cms-cades', cmd1 => 'cms', cmd2 => 'cms',
                tests => [ @smime_cms_cades_tests ]);
};

subtest "CAdES; cms incompatible arguments tests\n" => sub {
    plan tests => (scalar @smime_cms_cades_invalid_option_tests);

    foreach (@smime_cms_cades_invalid_option_tests) {
        ok(!run(app(["openssl", "cms", @{$$_[0]} ] )));
    }
};

subtest "CAdES ko tests\n" => sub {
    plan tests => 2 * scalar @smime_cms_cades_ko_tests;

    foreach (@smime_cms_cades_ko_tests) {
      SKIP: {
        my $skip_reason = check_availability($$_[0]);
        skip $skip_reason, 1 if $skip_reason;
        1 while unlink "cades-ko.txt";

        ok(run(app(["openssl", "cms", @{$$_[1]}])), $$_[0]);
        ok(!run(app(["openssl", "cms", @{$$_[3]}])), $$_[2]);
        }
    }
};

subtest "CMS binary input tests\n" => sub {
    my $input = srctop_file("test", "smcont.bin");
    my $signed = "smcont.signed";
    my $verified = "smcont.verified";

    plan tests => 11;

    ok(run(app(["openssl", "cms", "-sign", "-md", "sha256", "-signer", $smrsa1,
                "-binary", "-in", $input, "-out", $signed])),
       "sign binary input with -binary");
    ok(run(app(["openssl", "cms", "-verify", "-CAfile", $smroot,
                "-binary", "-in", $signed, "-out", $verified])),
       "verify binary input with -binary");
    is(compare($input, $verified), 0, "binary input retained with -binary");

    ok(run(app(["openssl", "cms", "-sign", "-md", "sha256", "-signer", $smrsa1,
                "-in", $input, "-out", $signed.".nobin"])),
       "sign binary input without -binary");
    ok(run(app(["openssl", "cms", "-verify", "-CAfile", $smroot,
                "-in", $signed.".nobin", "-out", $verified.".nobin"])),
       "verify binary input without -binary");
    is(compare($input, $verified.".nobin"), 1, "binary input not retained without -binary");
    ok(!run(app(["openssl", "cms", "-verify", "-CAfile", $smroot, "-crlfeol",
                "-binary", "-in", $signed, "-out", $verified.".crlfeol"])),
       "verify binary input wrong crlfeol");

    ok(run(app(["openssl", "cms", "-sign", "-md", "sha256", "-signer", $smrsa1,
                "-crlfeol",
                "-binary", "-in", $input, "-out", $signed.".crlf"])),
       "sign binary input with -binary -crlfeol");
    ok(run(app(["openssl", "cms", "-verify", "-CAfile", $smroot, "-crlfeol",
                "-binary", "-in", $signed.".crlf", "-out", $verified.".crlf"])),
       "verify binary input with -binary -crlfeol");
    is(compare($input, $verified.".crlf"), 0,
       "binary input retained with -binary -crlfeol");
    ok(!run(app(["openssl", "cms", "-verify", "-CAfile", $smroot,
                "-binary", "-in", $signed.".crlf", "-out", $verified.".crlf2"])),
       "verify binary input with -binary missing -crlfeol");
};

subtest "CMS signed digest, DER format" => sub {
    plan tests => 2;

    # Pre-computed SHA256 digest of $smcont in hexadecimal form
    my $digest = "ff236ef61b396355f75a4cc6e1c306d4c309084ae271a9e2ad6888f10a101b32";

    my $sig_file = "signature.der";
    ok(run(app(["openssl", "cms", @prov, "-sign", "-digest", $digest,
                    "-outform", "DER",
                    "-certfile", catfile($smdir, "smroot.pem"),
                    "-signer", catfile($smdir, "smrsa1.pem"),
                    "-out", $sig_file])),
        "CMS sign pre-computed digest, DER format");

    ok(run(app(["openssl", "cms", @prov, "-verify", "-in", $sig_file,
                    "-inform", "DER",
                    "-CAfile", catfile($smdir, "smroot.pem"),
                    "-content", $smcont])),
       "Verify CMS signed digest, DER format");
};

subtest "CMS signed digest, DER format, no signing time" => sub {
    # This test also enables CAdES mode and disables S/MIME capabilities
    # to approximate the kind of signature required for a PAdES-compliant
    # PDF signature.
    plan tests => 4;

    # Pre-computed SHA256 digest of $smcont in hexadecimal form
    my $digest = "ff236ef61b396355f75a4cc6e1c306d4c309084ae271a9e2ad6888f10a101b32";

    my $sig_file = "signature.der";
    ok(run(app(["openssl", "cms", @prov, "-sign", "-digest", $digest,
                    "-outform", "DER",
                    "-no_signing_time",
                    "-nosmimecap",
                    "-cades",
                    "-certfile", catfile($smdir, "smroot.pem"),
                    "-signer", catfile($smdir, "smrsa1.pem"),
                    "-out", $sig_file])),
        "CMS sign pre-computed digest, DER format, no signing time");

    my $exit = 0;
    my $dump = join "\n",
               run(app(["openssl", "cms", @prov, "-cmsout", "-noout", "-print",
                            "-in", $sig_file,
                            "-inform", "DER"]),
                   capture => 1,
                   statusvar => $exit);

    is($exit, 0, "Parse CMS signed digest, DER format, no signing time");
    is(index($dump, 'signingTime'), -1,
        "Check that CMS signed digest does not contain signing time");

    ok(run(app(["openssl", "cms", @prov, "-verify", "-in", $sig_file,
                    "-inform", "DER",
                    "-CAfile", catfile($smdir, "smroot.pem"),
                    "-content", $smcont])),
       "Verify CMS signed digest, DER format, no signing time");
};


subtest "CMS signed digest, S/MIME format" => sub {
    plan tests => 2;

    # Pre-computed SHA256 digest of $smcont in hexadecimal form
    my $digest = "ff236ef61b396355f75a4cc6e1c306d4c309084ae271a9e2ad6888f10a101b32";

    my $sig_file = "signature.smime";
    ok(run(app(["openssl", "cms", @prov, "-sign", "-digest", $digest,
                    "-outform", "SMIME",
                    "-certfile", catfile($smdir, "smroot.pem"),
                    "-signer", catfile($smdir, "smrsa1.pem"),
                    "-out", $sig_file])),
        "CMS sign pre-computed digest, S/MIME format");

    ok(run(app(["openssl", "cms", @prov, "-verify", "-in", $sig_file,
                    "-inform", "SMIME",
                    "-CAfile", catfile($smdir, "smroot.pem"),
                    "-content", $smcont])),
       "Verify CMS signed digest, S/MIME format");
};

sub path_tests {
    our $app = shift;
    our @path = qw(test certs);
    our $key = srctop_file(@path, "ee-key.pem");
    our $ee = srctop_file(@path, "ee-cert.pem");
    our $ca = srctop_file(@path, "ca-cert.pem");
    our $root = srctop_file(@path, "root-cert.pem");
    our $sig_file = "signature.p7s";

    sub sign {
        my $inter = shift;
        my @inter = $inter ? ("-certfile", $inter) : ();
        my $msg = shift;
        ok(run(app(["openssl", $app, @prov, "-sign", "-in", $smcont,
                    "-inkey", $key, "-signer", $ee, @inter,
                    "-out", $sig_file],
                   "accept $app sign with EE $msg".
                   " intermediate CA certificates")));
    }
    sub verify {
        my $inter = shift;
        my @inter = $inter ? ("-certfile", $inter) : ();
        my $msg = shift;
        my $res = shift;
        ok($res == run(app(["openssl", $app, @prov, "-verify", "-in", $sig_file,
                            "-purpose", "sslserver", "-CAfile", $root, @inter,
                            "-content", $smcont],
                           "accept $app verify with EE ".
                           "$msg intermediate CA certificates")));
    }
    sign($ca, "and");
    verify(0, "with included", 1);
    sign(0, "without");
    verify(0, "without", 0);
    verify($ca, "with added", 1);
};
subtest "CMS sign+verify cert path tests" => sub {
    plan tests => 5;

    path_tests("cms");
};
subtest "PKCS7 sign+verify cert path tests" => sub {
    plan tests => 5;

    path_tests("smime");
};

subtest "CMS code signing test" => sub {
    plan tests => 7;
    my $sig_file = "signature.p7s";
    ok(run(app(["openssl", "cms", @prov, "-sign", "-in", $smcont,
                   "-certfile", catfile($smdir, "smroot.pem"),
                   "-signer", catfile($smdir, "smrsa1.pem"),
                   "-out", $sig_file])),
       "accept perform CMS signature with smime certificate");

    ok(run(app(["openssl", "cms", @prov, "-verify", "-in", $sig_file,
                    "-CAfile", catfile($smdir, "smroot.pem"),
                    "-content", $smcont])),
       "accept verify CMS signature with smime certificate");

    ok(!run(app(["openssl", "cms", @prov, "-verify", "-in", $sig_file,
                    "-CAfile", catfile($smdir, "smroot.pem"),
                    "-purpose", "codesign",
                    "-content", $smcont])),
       "fail verify CMS signature with smime certificate for purpose code signing");

    ok(!run(app(["openssl", "cms", @prov, "-verify", "-in", $sig_file,
                    "-CAfile", catfile($smdir, "smroot.pem"),
                    "-purpose", "football",
                    "-content", $smcont])),
       "fail verify CMS signature with invalid purpose argument");

    ok(run(app(["openssl", "cms", @prov, "-sign", "-in", $smcont,
                   "-certfile", catfile($smdir, "smroot.pem"),
                   "-signer", catfile($smdir, "csrsa1.pem"),
                   "-out", $sig_file])),
        "accept perform CMS signature with code signing certificate");

    ok(run(app(["openssl", "cms", @prov, "-verify", "-in", $sig_file,
                    "-CAfile", catfile($smdir, "smroot.pem"),
                    "-purpose", "codesign",
                    "-content", $smcont])),
       "accept verify CMS signature with code signing certificate for purpose code signing");

    ok(!run(app(["openssl", "cms", @prov, "-verify", "-in", $sig_file,
                    "-CAfile", catfile($smdir, "smroot.pem"),
                    "-content", $smcont])),
       "fail verify CMS signature with code signing certificate for purpose smime_sign");
};

# Test case for missing MD algorithm (must not segfault)

with({ exit_checker => sub { return shift == 4; } },
    sub {
        ok(run(app(['openssl', 'smime', '-verify', '-noverify',
                    '-inform', 'PEM',
                    '-in', data_file("pkcs7-md4.pem"),
                   ])),
            "Check failure of EVP_DigestInit in PKCS7 signed is handled");

        ok(run(app(['openssl', 'smime', '-decrypt',
                    '-inform', 'PEM',
                    '-in', data_file("pkcs7-md4-encrypted.pem"),
                    '-recip', srctop_file("test", "certs", "ee-cert.pem"),
                    '-inkey', srctop_file("test", "certs", "ee-key.pem")
                   ])),
            "Check failure of EVP_DigestInit in PKCS7 signedAndEnveloped is handled");
    });

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

# Test case for the locking problem reported in #19643.
# This will fail if the fix is in and deadlock on Windows (and possibly
# other platforms) if not.
ok(!run(app(['openssl', 'cms', '-verify',
             '-CAfile', srctop_file("test/certs", "pkitsta.pem"),
             '-policy', 'anyPolicy',
             '-in', srctop_file("test/smime-eml",
                                "SignedInvalidMappingFromanyPolicyTest7.eml")
            ])),
   "issue#19643");

# Check that kari encryption with originator does not segfault
with({ exit_checker => sub { return shift == 3; } },
  sub {
    SKIP: {
      skip "EC is not supported in this build", 1 if $no_ec;

      ok(run(app(['openssl', 'cms', '-encrypt',
                  '-in', srctop_file("test", "smcont.txt"), '-aes128',
                  '-recip', catfile($smdir, "smec1.pem"),
                  '-originator', catfile($smdir, "smec3.pem"),
                  '-inkey', catfile($smdir, "smec3.pem")
                ])),
          "Check failure for currently not supported kari encryption with static originator");
    }
  });

# Check that we get the expected failure return code
with({ exit_checker => sub { return shift == 6; } },
    sub {
        ok(run(app(['openssl', 'cms', '-encrypt',
                    '-in', srctop_file("test", "smcont.txt"),
                    '-aes128', '-stream', '-recip',
                    srctop_file("test/smime-certs", "badrsa.pem"),
                   ])),
            "Check failure during BIO setup with -stream is handled correctly");
    });

# Test case for return value mis-check reported in #21986
with({ exit_checker => sub { return shift == 3; } },
    sub {
        SKIP: {
          skip "DSA is not supported in this build", 1 if $no_dsa;

          ok(run(app(['openssl', 'cms', '-sign',
                      '-in', srctop_file("test", "smcont.txt"),
                      '-signer', srctop_file("test/smime-certs", "smdsa1.pem"),
                      '-md', 'SHAKE256'])),
            "issue#21986");
        }
    });

# Test for problem reported in #22225
with({ exit_checker => sub { return shift == 3; } },
    sub {
	ok(run(app(['openssl', 'cms', '-encrypt',
		    '-in', srctop_file("test", "smcont.txt"),
		    '-aes-256-ctr', '-recip',
		    catfile($smdir, "smec1.pem"),
		   ])),
	   "Check for failure when cipher does not have an assigned OID (issue#22225)");
     });

# Test encrypt to three recipients, and decrypt using key-only;
# i.e. do not follow the recommended practice of providing the
# recipient cert in the decrypt op.
#
# Use RSAES-OAEP for key-transport, not RSAES-PKCS-v1_5.
#
# Because the cert is not provided during decrypt, all RSA ciphertexts
# are decrypted in turn, and when/if there is a valid decryption, it
# is assumed the correct content-key has been recovered.
#
# That process may fail with RSAES-PKCS-v1_5 b/c there is a
# non-negligible chance that decrypting a random input using
# RSAES-PKCS-v1_5 can result in a valid plaintext (so two content-keys
# could be recovered and the wrong one might be used).
#
# See https://github.com/openssl/project/issues/380
subtest "encrypt to three recipients with RSA-OAEP, key only decrypt" => sub {
    plan tests => 3;

    my $pt = srctop_file("test", "smcont.txt");
    my $ct = "smtst.cms";
    my $ptpt = "smtst.txt";

    ok(run(app(['openssl', 'cms',
		@defaultprov,
		'-encrypt', '-aes128',
		'-in', $pt,
		'-out', $ct,
		'-stream',
		'-recip', catfile($smdir, "smrsa1.pem"),
		'-keyopt', 'rsa_padding_mode:oaep',
		'-recip', catfile($smdir, "smrsa2.pem"),
		'-keyopt', 'rsa_padding_mode:oaep',
		'-recip', catfile($smdir, "smrsa3-cert.pem"),
		'-keyopt', 'rsa_padding_mode:oaep',
	       ])),
       "encrypt to three recipients with RSA-OAEP (avoid openssl/project issue#380)");
    ok(run(app(['openssl', 'cms',
		@defaultprov,
		'-decrypt', '-aes128',
		'-in', $ct,
		'-out', $ptpt,
		'-inkey', catfile($smdir, "smrsa3-key.pem"),
	       ])),
       "decrypt with key only");
    is(compare($pt, $ptpt), 0, "compare original message with decrypted ciphertext");
};

subtest "EdDSA tests for CMS" => sub {
    plan tests => 2;

    SKIP: {
        skip "ECX (EdDSA) is not supported in this build", 2
            if disabled("ecx");

        my $crt1 = srctop_file("test", "certs", "root-ed25519.pem");
        my $key1 = srctop_file("test", "certs", "root-ed25519.privkey.pem");
        my $sig1 = "sig1.cms";

        ok(run(app(["openssl", "cms", @prov, "-sign", "-md", "sha512", "-in", $smcont,
                    "-signer", $crt1, "-inkey", $key1, "-out", $sig1])),
           "accept CMS signature with Ed25519");

        ok(run(app(["openssl", "cms", @prov, "-verify", "-in", $sig1,
                    "-CAfile", $crt1, "-content", $smcont])),
           "accept CMS verify with Ed25519");
    }
};

subtest "ML-DSA tests for CMS" => sub {
    plan tests => 2;

    SKIP: {
        skip "ML-DSA is not supported in this build", 2
            if disabled("ml-dsa") || $no_pqc;

        my $sig1 = "sig1.cms";

        # draft-ietf-lamps-cms-ml-dsa: use SHA512 with ML-DSA
        ok(run(app(["openssl", "cms", @prov, "-sign", "-md", "sha512", "-in", $smcont,
                    "-certfile", $smroot, "-signer", catfile($smdir, "sm_mldsa44.pem"),
                    "-out", $sig1])),
           "accept CMS signature with ML-DSA-44");

        ok(run(app(["openssl", "cms", @prov, "-verify", "-in", $sig1,
                    "-CAfile", $smroot, "-content", $smcont])),
           "accept CMS verify with ML-DSA-44");
    }
};

subtest "SLH-DSA tests for CMS" => sub {
    plan tests => 6;

    SKIP: {
        skip "SLH-DSA is not supported in this build", 6
            if disabled("slh-dsa") || $no_pqc;

        my $sig1 = "sig1.cms";

        # draft-ietf-lamps-cms-sphincs-plus: use SHA512 with SLH-DSA-SHA2
        ok(run(app(["openssl", "cms", @prov, "-sign", "-md", "sha512", "-in", $smcont,
                    "-certfile", $smroot, "-signer", catfile($smdir, "sm_slhdsa_sha2_128s.pem"),
                    "-out", $sig1])),
           "accept CMS signature with SLH-DSA-SHA2-128s");

        ok(run(app(["openssl", "cms", @prov, "-verify", "-in", $sig1,
                    "-CAfile", $smroot, "-content", $smcont])),
           "accept CMS verify with SLH-DSA-SHA2-128s");

        # draft-ietf-lamps-cms-sphincs-plus: use SHAKE128 with SLH-DSA-SHAKE-128*
        ok(run(app(["openssl", "cms", @prov, "-sign", "-md", "shake128", "-in", $smcont,
                    "-certfile", $smroot, "-signer", catfile($smdir, "sm_slhdsa_shake_128s.pem"),
                    "-out", $sig1])),
           "accept CMS signature with SLH-DSA-SHAKE-128s");

        ok(run(app(["openssl", "cms", @prov, "-verify", "-in", $sig1,
                    "-CAfile", $smroot, "-content", $smcont])),
           "accept CMS verify with SLH-DSA-SHAKE-128s");

        # draft-ietf-lamps-cms-sphincs-plus: use SHAKE256 with SLH-DSA-SHAKE-256*
        ok(run(app(["openssl", "cms", @prov, "-sign", "-md", "shake256", "-in", $smcont,
                    "-certfile", $smroot, "-signer", catfile($smdir, "sm_slhdsa_shake_256s.pem"),
                    "-out", $sig1])),
           "accept CMS signature with SLH-DSA-SHAKE-256s");

        ok(run(app(["openssl", "cms", @prov, "-verify", "-in", $sig1,
                    "-CAfile", $smroot, "-content", $smcont])),
           "accept CMS verify with SLH-DSA-SHAKE-256s");
    }
};
