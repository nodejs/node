#! /usr/bin/env perl
# Copyright 2015-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use POSIX;
use File::Spec::Functions qw/devnull catfile/;
use File::Basename;
use File::Copy;
use OpenSSL::Test qw/:DEFAULT with pipe srctop_dir data_file/;
use OpenSSL::Test::Utils;

setup("test_ocsp");

plan skip_all => "OCSP is not supported by this OpenSSL build"
    if disabled("ocsp");

my $ocspdir=srctop_dir("test", "ocsp-tests");
# 17 December 2012 so we don't get certificate expiry errors.
my @check_time=("-attime", "1355875200");

sub test_ocsp {
    my $title = shift;
    my $inputfile = shift;
    my $CAfile = shift;
    my $untrusted = shift;
    if ($untrusted eq "") {
        $untrusted = $CAfile;
    }
    my $expected_exit = shift;
    my $nochecks = shift;
    my $outputfile = basename($inputfile, '.ors') . '.dat';

    run(app(["openssl", "base64", "-d",
             "-in", catfile($ocspdir,$inputfile),
             "-out", $outputfile]));
    with({ exit_checker => sub { return shift == $expected_exit; } },
         sub { ok(run(app(["openssl", "ocsp", "-respin", $outputfile,
                           "-partial_chain", @check_time,
                           "-CAfile", catfile($ocspdir, $CAfile),
                           "-verify_other", catfile($ocspdir, $untrusted),
                           "-no-CApath", "-no-CAstore",
                           $nochecks ? "-no_cert_checks" : ()])),
                  $title); });
}

plan tests => 11;

subtest "=== VALID OCSP RESPONSES ===" => sub {
    plan tests => 7;

    test_ocsp("NON-DELEGATED; Intermediate CA -> EE",
              "ND1.ors", "ND1_Issuer_ICA.pem", "", 0, 0);
    test_ocsp("NON-DELEGATED; Root CA -> Intermediate CA",
              "ND2.ors", "ND2_Issuer_Root.pem", "", 0, 0);
    test_ocsp("NON-DELEGATED; Root CA -> EE",
              "ND3.ors", "ND3_Issuer_Root.pem", "", 0, 0);
    test_ocsp("NON-DELEGATED; 3-level CA hierarchy",
              "ND1.ors", "ND1_Cross_Root.pem", "ND1_Issuer_ICA-Cross.pem", 0, 0);
    test_ocsp("DELEGATED; Intermediate CA -> EE",
              "D1.ors", "D1_Issuer_ICA.pem", "", 0, 0);
    test_ocsp("DELEGATED; Root CA -> Intermediate CA",
              "D2.ors", "D2_Issuer_Root.pem", "", 0, 0);
    test_ocsp("DELEGATED; Root CA -> EE",
              "D3.ors", "D3_Issuer_Root.pem", "", 0, 0);
};

subtest "=== INVALID SIGNATURE on the OCSP RESPONSE ===" => sub {
    plan tests => 6;

    test_ocsp("NON-DELEGATED; Intermediate CA -> EE",
              "ISOP_ND1.ors", "ND1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("NON-DELEGATED; Root CA -> Intermediate CA",
              "ISOP_ND2.ors", "ND2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("NON-DELEGATED; Root CA -> EE",
              "ISOP_ND3.ors", "ND3_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Intermediate CA -> EE",
              "ISOP_D1.ors", "D1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> Intermediate CA",
              "ISOP_D2.ors", "D2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> EE",
              "ISOP_D3.ors", "D3_Issuer_Root.pem", "", 1, 0);
};

subtest "=== WRONG RESPONDERID in the OCSP RESPONSE ===" => sub {
    plan tests => 6;

    test_ocsp("NON-DELEGATED; Intermediate CA -> EE",
              "WRID_ND1.ors", "ND1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("NON-DELEGATED; Root CA -> Intermediate CA",
              "WRID_ND2.ors", "ND2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("NON-DELEGATED; Root CA -> EE",
              "WRID_ND3.ors", "ND3_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Intermediate CA -> EE",
              "WRID_D1.ors", "D1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> Intermediate CA",
              "WRID_D2.ors", "D2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> EE",
              "WRID_D3.ors", "D3_Issuer_Root.pem", "", 1, 0);
};

subtest "=== WRONG ISSUERNAMEHASH in the OCSP RESPONSE ===" => sub {
    plan tests => 6;

    test_ocsp("NON-DELEGATED; Intermediate CA -> EE",
              "WINH_ND1.ors", "ND1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("NON-DELEGATED; Root CA -> Intermediate CA",
              "WINH_ND2.ors", "ND2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("NON-DELEGATED; Root CA -> EE",
              "WINH_ND3.ors", "ND3_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Intermediate CA -> EE",
              "WINH_D1.ors", "D1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> Intermediate CA",
              "WINH_D2.ors", "D2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> EE",
              "WINH_D3.ors", "D3_Issuer_Root.pem", "", 1, 0);
};

subtest "=== WRONG ISSUERKEYHASH in the OCSP RESPONSE ===" => sub {
    plan tests => 6;

    test_ocsp("NON-DELEGATED; Intermediate CA -> EE",
              "WIKH_ND1.ors", "ND1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("NON-DELEGATED; Root CA -> Intermediate CA",
              "WIKH_ND2.ors", "ND2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("NON-DELEGATED; Root CA -> EE",
              "WIKH_ND3.ors", "ND3_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Intermediate CA -> EE",
              "WIKH_D1.ors", "D1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> Intermediate CA",
              "WIKH_D2.ors", "D2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> EE",
              "WIKH_D3.ors", "D3_Issuer_Root.pem", "", 1, 0);
};

subtest "=== WRONG KEY in the DELEGATED OCSP SIGNING CERTIFICATE ===" => sub {
    plan tests => 3;

    test_ocsp("DELEGATED; Intermediate CA -> EE",
              "WKDOSC_D1.ors", "D1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> Intermediate CA",
              "WKDOSC_D2.ors", "D2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> EE",
              "WKDOSC_D3.ors", "D3_Issuer_Root.pem", "", 1, 0);
};

subtest "=== INVALID SIGNATURE on the DELEGATED OCSP SIGNING CERTIFICATE ===" => sub {
    plan tests => 6;

    test_ocsp("DELEGATED; Intermediate CA -> EE",
              "ISDOSC_D1.ors", "D1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> Intermediate CA",
              "ISDOSC_D2.ors", "D2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> EE",
              "ISDOSC_D3.ors", "D3_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Intermediate CA -> EE",
              "ISDOSC_D1.ors", "D1_Issuer_ICA.pem", "", 1, 1);
    test_ocsp("DELEGATED; Root CA -> Intermediate CA",
              "ISDOSC_D2.ors", "D2_Issuer_Root.pem", "", 1, 1);
    test_ocsp("DELEGATED; Root CA -> EE",
              "ISDOSC_D3.ors", "D3_Issuer_Root.pem", "", 1, 1);
};

subtest "=== WRONG SUBJECT NAME in the ISSUER CERTIFICATE ===" => sub {
    plan tests => 6;

    test_ocsp("NON-DELEGATED; Intermediate CA -> EE",
              "ND1.ors", "WSNIC_ND1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("NON-DELEGATED; Root CA -> Intermediate CA",
              "ND2.ors", "WSNIC_ND2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("NON-DELEGATED; Root CA -> EE",
              "ND3.ors", "WSNIC_ND3_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Intermediate CA -> EE",
              "D1.ors", "WSNIC_D1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> Intermediate CA",
              "D2.ors", "WSNIC_D2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> EE",
              "D3.ors", "WSNIC_D3_Issuer_Root.pem", "", 1, 0);
};

subtest "=== WRONG KEY in the ISSUER CERTIFICATE ===" => sub {
    plan tests => 6;

    test_ocsp("NON-DELEGATED; Intermediate CA -> EE",
              "ND1.ors", "WKIC_ND1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("NON-DELEGATED; Root CA -> Intermediate CA",
              "ND2.ors", "WKIC_ND2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("NON-DELEGATED; Root CA -> EE",
              "ND3.ors", "WKIC_ND3_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Intermediate CA -> EE",
              "D1.ors", "WKIC_D1_Issuer_ICA.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> Intermediate CA",
              "D2.ors", "WKIC_D2_Issuer_Root.pem", "", 1, 0);
    test_ocsp("DELEGATED; Root CA -> EE",
              "D3.ors", "WKIC_D3_Issuer_Root.pem", "", 1, 0);
};

subtest "=== INVALID SIGNATURE on the ISSUER CERTIFICATE ===" => sub {
    plan tests => 6;

    # Expect success, because we're explicitly trusting the issuer certificate.
    test_ocsp("NON-DELEGATED; Intermediate CA -> EE",
              "ND1.ors", "ISIC_ND1_Issuer_ICA.pem", "", 0, 0);
    test_ocsp("NON-DELEGATED; Root CA -> Intermediate CA",
              "ND2.ors", "ISIC_ND2_Issuer_Root.pem", "", 0, 0);
    test_ocsp("NON-DELEGATED; Root CA -> EE",
              "ND3.ors", "ISIC_ND3_Issuer_Root.pem", "", 0, 0);
    test_ocsp("DELEGATED; Intermediate CA -> EE",
              "D1.ors", "ISIC_D1_Issuer_ICA.pem", "", 0, 0);
    test_ocsp("DELEGATED; Root CA -> Intermediate CA",
              "D2.ors", "ISIC_D2_Issuer_Root.pem", "", 0, 0);
    test_ocsp("DELEGATED; Root CA -> EE",
              "D3.ors", "ISIC_D3_Issuer_Root.pem", "", 0, 0);
};

subtest "=== OCSP API TESTS===" => sub {
    plan tests => 1;

    ok(run(test(["ocspapitest", data_file("cert.pem"), data_file("key.pem")])),
                 "running ocspapitest");
}
