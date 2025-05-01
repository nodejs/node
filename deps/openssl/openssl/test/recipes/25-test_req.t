#! /usr/bin/env perl
# Copyright 2015-2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_req");

plan tests => 113;

require_ok(srctop_file('test', 'recipes', 'tconversion.pl'));

my @certs = qw(test certs);

# What type of key to generate?
my @req_new;
if (disabled("rsa")) {
    @req_new = ("-newkey", "dsa:".srctop_file("apps", "dsa512.pem"));
} else {
    @req_new = ("-new");
    note("There should be a 2 sequences of .'s and some +'s.");
    note("There should not be more that at most 80 per line");
}

# Prevent MSys2 filename munging for arguments that look like file paths but
# aren't
$ENV{MSYS2_ARG_CONV_EXCL} = "/CN=";

# Check for duplicate -addext parameters, and one "working" case.
my @addext_args = ( "openssl", "req", "-new", "-out", "testreq-addexts.pem",
                    "-key",  srctop_file(@certs, "ee-key.pem"),
    "-config", srctop_file("test", "test.cnf"), @req_new );
my $val = "subjectAltName=DNS:example.com";
my $val1 = "subjectAltName=otherName:1.2.3.4;UTF8:test,email:info\@example.com";
my $val2 = " " . $val;
my $val3 = $val;
$val3 =~ s/=/    =/;
ok( run(app([@addext_args, "-addext", $val])));
ok( run(app([@addext_args, "-addext", $val1])));
$val1 =~ s/UTF8/XXXX/; # execute the error handling in do_othername
ok(!run(app([@addext_args, "-addext", $val1])));
ok(!run(app([@addext_args, "-addext", $val, "-addext", $val])));
ok(!run(app([@addext_args, "-addext", $val, "-addext", $val2])));
ok(!run(app([@addext_args, "-addext", $val, "-addext", $val3])));
ok(!run(app([@addext_args, "-addext", $val2, "-addext", $val3])));
ok(run(app([@addext_args, "-addext", "SXNetID=1:one, 2:two, 3:three"])));
ok(run(app([@addext_args, "-addext", "subjectAltName=dirName:dirname_sec"])));

ok(run(app([@addext_args, "-addext", "keyUsage=digitalSignature",
           "-reqexts", "reqexts"]))); # referring to section in test.cnf

# If a CSR is provided with neither of -key or -CA/-CAkey, this should fail.
ok(!run(app(["openssl", "req", "-x509",
                "-in", srctop_file(@certs, "x509-check.csr"),
                "-out", "testreq.pem"])));

subtest "generating alt certificate requests with RSA" => sub {
    plan tests => 3;

    SKIP: {
        skip "RSA is not supported by this OpenSSL build", 2
            if disabled("rsa");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-section", "altreq",
                    "-new", "-out", "testreq-rsa.pem", "-utf8",
                    "-key", srctop_file("test", "testrsa.pem")])),
           "Generating request");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq-rsa.pem", "-noout"])),
           "Verifying signature on request");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-section", "altreq",
                    "-verify", "-in", "testreq-rsa.pem", "-noout"])),
           "Verifying signature on request");
    }
};


subtest "generating certificate requests with RSA" => sub {
    plan tests => 8;

    SKIP: {
        skip "RSA is not supported by this OpenSSL build", 2
            if disabled("rsa");

        ok(!run(app(["openssl", "req",
                     "-config", srctop_file("test", "test.cnf"),
                     "-new", "-out", "testreq-rsa.pem", "-utf8",
                     "-key", srctop_file("test", "testrsa.pem"),
                     "-keyform", "DER"])),
           "Checking that mismatching keyform fails");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new", "-out", "testreq-rsa.pem", "-utf8",
                    "-key", srctop_file("test", "testrsa.pem"),
                    "-keyform", "PEM"])),
           "Generating request");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq-rsa.pem", "-noout"])),
           "Verifying signature on request");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-modulus", "-in", "testreq-rsa.pem", "-noout"])),
           "Printing a modulus of the request key");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new", "-out", "testreq_withattrs_pem.pem", "-utf8",
                    "-key", srctop_file("test", "testrsa_withattrs.pem")])),
           "Generating request from a key with extra attributes - PEM");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq_withattrs_pem.pem", "-noout"])),
           "Verifying signature on request from a key with extra attributes - PEM");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new", "-out", "testreq_withattrs_der.pem", "-utf8",
                    "-key", srctop_file("test", "testrsa_withattrs.der"),
                    "-keyform", "DER"])),
           "Generating request from a key with extra attributes - PEM");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq_withattrs_der.pem", "-noout"])),
           "Verifying signature on request from a key with extra attributes - PEM");
    }
};

subtest "generating certificate requests with RSA-PSS" => sub {
    plan tests => 12;

    SKIP: {
        skip "RSA is not supported by this OpenSSL build", 2
            if disabled("rsa");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new", "-out", "testreq-rsapss.pem", "-utf8",
                    "-key", srctop_file("test", "testrsapss.pem")])),
           "Generating request");
        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq-rsapss.pem", "-noout"])),
           "Verifying signature on request");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new", "-out", "testreq-rsapss2.pem", "-utf8",
                    "-sigopt", "rsa_padding_mode:pss",
                    "-sigopt", "rsa_pss_saltlen:-1",
                    "-key", srctop_file("test", "testrsapss.pem")])),
           "Generating request");
        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq-rsapss2.pem", "-noout"])),
           "Verifying signature on request");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new", "-out", "testreq-rsapssmand.pem", "-utf8",
                    "-sigopt", "rsa_padding_mode:pss",
                    "-key", srctop_file("test", "testrsapssmandatory.pem")])),
           "Generating request");
        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq-rsapssmand.pem", "-noout"])),
           "Verifying signature on request");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new", "-out", "testreq-rsapssmand2.pem", "-utf8",
                    "-sigopt", "rsa_pss_saltlen:100",
                    "-key", srctop_file("test", "testrsapssmandatory.pem")])),
           "Generating request");
        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq-rsapssmand2.pem", "-noout"])),
           "Verifying signature on request");

        ok(!run(app(["openssl", "req",
                     "-config", srctop_file("test", "test.cnf"),
                     "-new", "-out", "testreq-rsapss3.pem", "-utf8",
                     "-sigopt", "rsa_padding_mode:pkcs1",
                     "-key", srctop_file("test", "testrsapss.pem")])),
           "Generating request with expected failure");

        ok(!run(app(["openssl", "req",
                     "-config", srctop_file("test", "test.cnf"),
                     "-new", "-out", "testreq-rsapss3.pem", "-utf8",
                     "-sigopt", "rsa_pss_saltlen:-5",
                     "-key", srctop_file("test", "testrsapss.pem")])),
           "Generating request with expected failure");

        ok(!run(app(["openssl", "req",
                     "-config", srctop_file("test", "test.cnf"),
                     "-new", "-out", "testreq-rsapssmand3.pem", "-utf8",
                     "-sigopt", "rsa_pss_saltlen:10",
                     "-key", srctop_file("test", "testrsapssmandatory.pem")])),
           "Generating request with expected failure");

        ok(!run(app(["openssl", "req",
                     "-config", srctop_file("test", "test.cnf"),
                     "-new", "-out", "testreq-rsapssmand3.pem", "-utf8",
                     "-sha256",
                     "-key", srctop_file("test", "testrsapssmandatory.pem")])),
           "Generating request with expected failure");
    }
};

subtest "generating certificate requests with DSA" => sub {
    plan tests => 2;

    SKIP: {
        skip "DSA is not supported by this OpenSSL build", 2
            if disabled("dsa");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new", "-out", "testreq-dsa.pem", "-utf8",
                    "-key", srctop_file("test", "testdsa.pem")])),
           "Generating request");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq-dsa.pem", "-noout"])),
           "Verifying signature on request");
    }
};

subtest "generating certificate requests with ECDSA" => sub {
    plan tests => 2;

    SKIP: {
        skip "ECDSA is not supported by this OpenSSL build", 2
            if disabled("ec");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new", "-out", "testreq-ec.pem", "-utf8",
                    "-key", srctop_file("test", "testec-p256.pem")])),
           "Generating request");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq-ec.pem", "-noout"])),
           "Verifying signature on request");
    }
};

subtest "generating certificate requests with Ed25519" => sub {
    plan tests => 2;

    SKIP: {
        skip "Ed25519 is not supported by this OpenSSL build", 2
            if disabled("ecx");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new", "-out", "testreq-ed25519.pem", "-utf8",
                    "-key", srctop_file("test", "tested25519.pem")])),
           "Generating request");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq-ed25519.pem", "-noout"])),
           "Verifying signature on request");
    }
};

subtest "generating certificate requests with Ed448" => sub {
    plan tests => 2;

    SKIP: {
        skip "Ed448 is not supported by this OpenSSL build", 2
            if disabled("ecx");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new", "-out", "testreq-ed448.pem", "-utf8",
                    "-key", srctop_file("test", "tested448.pem")])),
           "Generating request");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq-ed448.pem", "-noout"])),
           "Verifying signature on request");
    }
};

subtest "generating certificate requests" => sub {
    plan tests => 2;

    ok(run(app(["openssl", "req", "-config", srctop_file("test", "test.cnf"),
                "-key", srctop_file(@certs, "ee-key.pem"),
                @req_new, "-out", "testreq.pem"])),
       "Generating request");

    ok(run(app(["openssl", "req", "-config", srctop_file("test", "test.cnf"),
                "-verify", "-in", "testreq.pem", "-noout"])),
       "Verifying signature on request");
};

subtest "generating SM2 certificate requests" => sub {
    plan tests => 4;

    SKIP: {
        skip "SM2 is not supported by this OpenSSL build", 4
        if disabled("sm2");
        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new", "-key", srctop_file(@certs, "sm2.key"),
                    "-sigopt", "distid:1234567812345678",
                    "-out", "testreq-sm2.pem", "-sm3"])),
           "Generating SM2 certificate request");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq-sm2.pem", "-noout",
                    "-vfyopt", "distid:1234567812345678", "-sm3"])),
           "Verifying signature on SM2 certificate request");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new", "-key", srctop_file(@certs, "sm2.key"),
                    "-sigopt", "hexdistid:DEADBEEF",
                    "-out", "testreq-sm2.pem", "-sm3"])),
           "Generating SM2 certificate request with hex id");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-verify", "-in", "testreq-sm2.pem", "-noout",
                    "-vfyopt", "hexdistid:DEADBEEF", "-sm3"])),
           "Verifying signature on SM2 certificate request");
    }
};

subtest "generating certificate requests with ML-DSA" => sub {
    plan tests => 5;

    SKIP: {
        skip "ML-DSA is not supported by this OpenSSL build", 5
            if disabled("ml-dsa");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-x509", "-sha256", "-nodes", "-days", "365",
                    "-newkey", "ML-DSA-44",
                    "-keyout",  "privatekey_ml_dsa_44.pem",
                    "-out",  "cert_ml_dsa_44.pem",
                    "-subj", "/CN=test-self-signed",
                    "-addext","keyUsage=digitalSignature"])),
                    "Generating self signed ML-DSA-44 cert and private key");
        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-x509", "-sha256", "-nodes", "-days", "365",
                    "-newkey", "ML-DSA-65",
                    "-keyout",  "privatekey_ml_dsa_65.pem",
                    "-out",  "cert_ml_dsa_65.pem",
                    "-subj", "/CN=test-self-signed",
                    "-addext","keyUsage=digitalSignature"])),
                    "Generating self signed ML-DSA-65 cert and private key");
        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-x509", "-sha256", "-nodes", "-days", "365",
                    "-newkey", "ML-DSA-44",
                    "-keyout",  "privatekey_ml_dsa_87.pem",
                    "-out",  "cert_ml_dsa_87.pem",
                    "-subj", "/CN=test-self-signed",
                    "-addext","keyUsage=digitalSignature"])),
                    "Generating self signed ML-DSA-87 cert and private key");
        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new",
                    "-sigopt","hextest-entropy:000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
                    "-out", "csr_ml_dsa_87.pem",
                    "-newkey", "ML-DSA-87",
                    "-passout", "pass:x"])),
                    "Generating ML-DSA-87 csr");
        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-in", "csr_ml_dsa_87.pem"])),
                    "verifying ML-DSA-87 csr");
    }
};

subtest "generating certificate requests with -cipher flag" => sub {
    plan tests => 6;

    diag("Testing -cipher flag with aes-256-cbc...");
    ok(run(app(["openssl", "req",
                "-config", srctop_file("test", "test.cnf"),
                "-newkey", "rsa:2048",
                "-keyout", "privatekey-aes256.pem",
                "-out", "testreq-rsa-cipher.pem",
                "-utf8",
                "-cipher", "aes-256-cbc",
                "-passout", "pass:password"])),
       "Generating request with -cipher flag (AES-256-CBC)");

    diag("Verifying signature for aes-256-cbc...");
    ok(run(app(["openssl", "req",
                "-config", srctop_file("test", "test.cnf"),
                "-verify", "-in", "testreq-rsa-cipher.pem", "-noout"])),
       "Verifying signature on request with -cipher (AES-256-CBC)");

    open my $fh, '<', "privatekey-aes256.pem" or BAIL_OUT("Could not open key file: $!");
    my $first_line = <$fh>;
    close $fh;
    ok($first_line =~ /^-----BEGIN ENCRYPTED PRIVATE KEY-----/,
       "Check that the key file is encrypted (AES-256-CBC)");

    diag("Testing -cipher flag with aes-128-cbc...");
    ok(run(app(["openssl", "req",
                "-config", srctop_file("test", "test.cnf"),
                "-newkey", "rsa:2048",
                "-keyout", "privatekey-aes128.pem",
                "-out", "testreq-rsa-cipher-aes128.pem",
                "-utf8",
                "-cipher", "aes-128-cbc",
                "-passout", "pass:password"])),
       "Generating request with -cipher flag (AES-128-CBC)");

    diag("Verifying signature for aes-128-cbc...");
    ok(run(app(["openssl", "req",
                "-config", srctop_file("test", "test.cnf"),
                "-verify", "-in", "testreq-rsa-cipher-aes128.pem", "-noout"])),
       "Verifying signature on request with -cipher (AES-128-CBC)");

    open my $fh_aes128, '<', "privatekey-aes128.pem" or BAIL_OUT("Could not open key file: $!");
    my $first_line_aes128 = <$fh_aes128>;
    close $fh_aes128;
    ok($first_line_aes128 =~ /^-----BEGIN ENCRYPTED PRIVATE KEY-----/,
       "Check that the key file is encrypted (AES-128-CBC)");
};

subtest "generating certificate requests with SLH-DSA" => sub {
    plan tests => 5;

    SKIP: {
        skip "SLH-DSA is not supported by this OpenSSL build", 5
            if disabled("slh-dsa");

        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-x509", "-sha256", "-nodes", "-days", "365",
                    "-newkey", "SLH-DSA-SHA2-128f",
                    "-keyout",  "privatekey_slh_dsa_sha2_128f.pem",
                    "-out",  "cert_slh_dsa_sha2_128f.pem",
                    "-subj", "/CN=test-self-signed",
                    "-addext","keyUsage=digitalSignature"])),
                    "Generating self signed SLH-DSA-SHA2-128f cert and private key");
        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-x509", "-sha256", "-nodes", "-days", "365",
                    "-newkey", "SLH-DSA-SHA2-256s",
                    "-keyout",  "privatekey_slh_dsa_sha2_256s.pem",
                    "-out",  "cert_slh_dsa_sha2_256s.pem",
                    "-subj", "/CN=test-self-signed",
                    "-addext","keyUsage=digitalSignature"])),
                    "Generating self signed SLH-DSA-SHA2-256s cert and private key");
        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-x509", "-sha256", "-nodes", "-days", "365",
                    "-newkey", "SLH-DSA-SHAKE-256f",
                    "-keyout",  "privatekey_slh_dsa_shake_256f.pem",
                    "-out",  "cert_slh_dsa_shake_256f.pem",
                    "-subj", "/CN=test-self-signed",
                    "-addext","keyUsage=digitalSignature"])),
                    "Generating self signed SLH-DSA-SHAKE-256f cert and private key");
        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-new",
                    "-sigopt","hextest-entropy:000102030405060708090a0b0c0d0e0f",
                    "-out", "csr_slh_dsa_shake128.pem",
                    "-newkey", "SLH-DSA-SHAKE-128s",
                    "-passout", "pass:x"])),
                    "Generating SLH-DSA-SHAKE-128s csr");
        ok(run(app(["openssl", "req",
                    "-config", srctop_file("test", "test.cnf"),
                    "-in", "csr_slh_dsa_shake128.pem"])),
                    "verifying SLH-DSA-SHAKE-128s csr");
    }
};

my @openssl_args = ("req", "-config", srctop_file("apps", "openssl.cnf"));

run_conversion('req conversions',
               "testreq.pem");
run_conversion('req conversions -- testreq2',
               srctop_file("test", "testreq2.pem"));

sub run_conversion {
    my $title = shift;
    my $reqfile = shift;

    subtest $title => sub {
        run(app(["openssl", @openssl_args,
                 "-in", $reqfile, "-inform", "p",
                 "-noout", "-text"],
                stderr => "req-check.err", stdout => undef));
        open DATA, "req-check.err";
        SKIP: {
            plan skip_all => "skipping req conversion test for $reqfile"
                if grep /Unknown Public Key/, map { s/\R//; } <DATA>;

            tconversion( -type => 'req', -in => $reqfile,
                         -args => [ @openssl_args ] );
        }
        close DATA;
        unlink "req-check.err";

        done_testing();
    };
}

# Test both generation and verification of certs w.r.t. RFC 5280 requirements

my $ca_cert; # will be set below
sub generate_cert {
    my $cert = shift @_;
    my $ss = $cert =~ m/self-signed/;
    my $is_ca = $cert =~ m/CA/;
    my $cn = $is_ca ? "CA" : "EE";
    my $ca_key = srctop_file(@certs, "ca-key.pem");
    my $key = $is_ca ? $ca_key : srctop_file(@certs, "ee-key.pem");
    my @cmd = ("openssl", "req", "-config", "", "-x509",
               "-subj", "/CN=$cn", @_, "-out", $cert);
    push(@cmd, ("-key", $key)) if $ss;
    push(@cmd, ("-CA", $ca_cert, "-CAkey", $ca_key)) unless $ss;
    ok(run(app([@cmd])), "generate $cert");
}

sub has_keyUsage {
    my $cert = shift @_;
    my $expect = shift @_;
    cert_contains($cert, "Key Usage", $expect);
}
sub strict_verify {
    my $cert = shift @_;
    my $expect = shift @_;
    my $trusted = shift @_;
    $trusted = $cert unless $trusted;
    ok(run(app(["openssl", "verify", "-x509_strict", "-trusted", $trusted,
                "-partial_chain", $cert])) == $expect,
       "strict verify allow $cert");
}

my @v3_ca = ("-addext", "basicConstraints = critical,CA:true",
             "-addext", "keyUsage = keyCertSign");
my $SKID_AKID = "subjectKeyIdentifier,authorityKeyIdentifier";

# # SKID

my $cert = "self-signed_default_SKID_no_explicit_exts.pem";
generate_cert($cert);
has_version($cert, 3);
has_SKID($cert, 1); # SKID added, though no explicit extensions given
has_AKID($cert, 0);

my $cert = "self-signed_v3_CA_hash_SKID.pem";
generate_cert($cert, @v3_ca, "-addext", "subjectKeyIdentifier = hash");
has_SKID($cert, 1); # explicit hash SKID

$cert = "self-signed_v3_CA_no_SKID.pem";
generate_cert($cert, @v3_ca, "-addext", "subjectKeyIdentifier = none");
cert_ext_has_n_different_lines($cert, 0, $SKID_AKID); # no SKID and no AKID
#TODO strict_verify($cert, 0);

$cert = "self-signed_v3_CA_given_SKID.pem";
generate_cert($cert, @v3_ca, "-addext", "subjectKeyIdentifier = 45");
cert_contains($cert, "Subject Key Identifier: 45 ", 1); # given SKID
strict_verify($cert, 1);

# AKID of self-signed certs

$cert = "self-signed_v1_CA_no_KIDs.pem";
generate_cert($cert, "-x509v1");
has_version($cert, 1);
cert_ext_has_n_different_lines($cert, 0, $SKID_AKID); # no SKID and no AKID
#TODO strict_verify($cert, 1); # self-signed v1 root cert should be accepted as CA

$ca_cert = "self-signed_v3_CA_default_SKID.pem"; # will also be used below
generate_cert($ca_cert, @v3_ca);
has_SKID($ca_cert, 1); # default SKID
has_AKID($ca_cert, 0); # no default AKID
strict_verify($ca_cert, 1);

$cert = "self-signed_v3_CA_no_AKID.pem";
generate_cert($cert, @v3_ca, "-addext", "authorityKeyIdentifier = none");
has_AKID($cert, 0); # forced no AKID

$cert = "self-signed_v3_CA_explicit_AKID.pem";
generate_cert($cert, @v3_ca, "-addext", "authorityKeyIdentifier = keyid");
has_AKID($cert, 0); # for self-signed cert, AKID suppressed and not forced

$cert = "self-signed_v3_CA_forced_AKID.pem";
generate_cert($cert, @v3_ca, "-addext", "authorityKeyIdentifier = keyid:always");
cert_ext_has_n_different_lines($cert, 3, $SKID_AKID); # forced AKID, AKID == SKID
strict_verify($cert, 1);

$cert = "self-signed_v3_CA_issuer_AKID.pem";
generate_cert($cert, @v3_ca, "-addext", "authorityKeyIdentifier = issuer");
has_AKID($cert, 0); # suppressed AKID since not forced

$cert = "self-signed_v3_CA_forced_issuer_AKID.pem";
generate_cert($cert, @v3_ca, "-addext", "authorityKeyIdentifier = issuer:always");
cert_contains($cert, "Authority Key Identifier: DirName:/CN=CA serial:", 1); # forced issuer AKID

$cert = "self-signed_v3_CA_nonforced_keyid_issuer_AKID.pem";
generate_cert($cert, @v3_ca, "-addext", "authorityKeyIdentifier = keyid, issuer");
has_AKID($cert, 0); # AKID not present because not forced and cert self-signed

$cert = "self-signed_v3_CA_keyid_forced_issuer_AKID.pem";
generate_cert($cert, @v3_ca, "-addext", "authorityKeyIdentifier = keyid, issuer:always");
cert_contains($cert, "Authority Key Identifier: DirName:/CN=CA serial:", 1); # issuer AKID forced, with keyid not forced

$cert = "self-signed_v3_CA_forced_keyid_issuer_AKID.pem";
generate_cert($cert, @v3_ca, "-addext", "authorityKeyIdentifier = keyid:always, issuer");
has_AKID($cert, 1); # AKID with keyid forced
cert_contains($cert, "Authority Key Identifier: DirName:/CN=CA serial:", 0); # no issuer AKID

$cert = "self-signed_v3_CA_forced_keyid_forced_issuer_AKID.pem";
generate_cert($cert, @v3_ca, "-addext", "authorityKeyIdentifier = keyid:always, issuer:always");
cert_contains($cert, "Authority Key Identifier: keyid(:[0-9A-Fa-f]{2})+ DirName:/CN=CA serial:", 1); # AKID with keyid and issuer forced

$cert = "self-signed_v3_EE_wrong_keyUsage.pem";
generate_cert($cert, "-addext", "keyUsage = keyCertSign");
#TODO strict_verify($cert, 1); # should be accepted because RFC 5280 does not apply

# AKID of self-issued but not self-signed certs

$cert = "self-issued_x509_v3_CA_default_KIDs.pem";
ok(run(app([("openssl", "x509", "-copy_extensions", "copy",
             "-req", "-in", srctop_file(@certs, "ext-check.csr"),
             "-key", srctop_file(@certs, "ca-key.pem"),
             "-force_pubkey", srctop_file("test", "testrsapub.pem"),
             "-out", $cert)])), "generate using x509: $cert");
cert_contains($cert, "Issuer: CN=test .*? Subject: CN=test", 1);
cert_ext_has_n_different_lines($cert, 4, $SKID_AKID); # SKID != AKID
strict_verify($cert, 1);

$cert = "self-issued_v3_CA_default_KIDs.pem";
generate_cert($cert, "-addext", "keyUsage = dataEncipherment",
    "-in", srctop_file(@certs, "x509-check.csr"));
cert_contains($cert, "Issuer: CN=CA .*? Subject: CN=CA", 1);
cert_ext_has_n_different_lines($cert, 4, $SKID_AKID); # SKID != AKID
strict_verify($cert, 1);

$cert = "self-issued_v3_CA_no_AKID.pem";
generate_cert($cert, "-addext", "authorityKeyIdentifier = none",
    "-in", srctop_file(@certs, "x509-check.csr"));
has_version($cert, 3);
has_SKID($cert, 1); # SKID added, though no explicit extensions given
has_AKID($cert, 0);
strict_verify($cert, 1);

$cert = "self-issued_v3_CA_explicit_AKID.pem";
generate_cert($cert, "-addext", "authorityKeyIdentifier = keyid",
    "-in", srctop_file(@certs, "x509-check.csr"));
cert_ext_has_n_different_lines($cert, 4, $SKID_AKID); # SKID != AKID
strict_verify($cert, 1);

$cert = "self-issued_v3_CA_forced_AKID.pem";
generate_cert($cert, "-addext", "authorityKeyIdentifier = keyid:always",
    "-in", srctop_file(@certs, "x509-check.csr"));
cert_ext_has_n_different_lines($cert, 4, $SKID_AKID); # SKID != AKID

$cert = "self-issued_v3_CA_issuer_AKID.pem";
generate_cert($cert, @v3_ca, "-addext", "authorityKeyIdentifier = issuer",
    "-in", srctop_file(@certs, "x509-check.csr"));
cert_contains($cert, "Authority Key Identifier: DirName:/CN=CA serial:", 1); # just issuer AKID

$cert = "self-issued_v3_CA_forced_issuer_AKID.pem";
generate_cert($cert, @v3_ca, "-addext", "authorityKeyIdentifier = issuer:always",
    "-in", srctop_file(@certs, "x509-check.csr"));
cert_contains($cert, "Authority Key Identifier: DirName:/CN=CA serial:", 1); # just issuer AKID

$cert = "self-issued_v3_CA_keyid_issuer_AKID.pem";
generate_cert($cert, "-addext", "authorityKeyIdentifier = keyid, issuer",
    "-in", srctop_file(@certs, "x509-check.csr"));
cert_ext_has_n_different_lines($cert, 4, $SKID_AKID); # SKID != AKID, not forced

$cert = "self-issued_v3_CA_keyid_forced_issuer_AKID.pem";
generate_cert($cert, "-addext", "authorityKeyIdentifier = keyid, issuer:always",
    "-in", srctop_file(@certs, "x509-check.csr"));
cert_ext_has_n_different_lines($cert, 6, $SKID_AKID); # SKID != AKID, with forced issuer

$cert = "self-issued_v3_CA_forced_keyid_and_issuer_AKID.pem";
generate_cert($cert, "-addext", "authorityKeyIdentifier = keyid:always, issuer:always",
    "-in", srctop_file(@certs, "x509-check.csr"));
cert_ext_has_n_different_lines($cert, 6, $SKID_AKID); # SKID != AKID, both forced

# AKID of not self-issued certs

$cert = "regular_v3_EE_default_KIDs_no_other_exts.pem";
generate_cert($cert, "-key", srctop_file(@certs, "ee-key.pem"));
has_version($cert, 3);
cert_ext_has_n_different_lines($cert, 4, $SKID_AKID); # SKID != AKID

$cert = "regular_v3_EE_default_KIDs.pem";
generate_cert($cert, "-addext", "keyUsage = dataEncipherment",
    "-key", srctop_file(@certs, "ee-key.pem"));
cert_ext_has_n_different_lines($cert, 4, $SKID_AKID); # SKID != AKID
strict_verify($cert, 1, $ca_cert);

$cert = "regular_v3_EE_copied_exts_default_KIDs.pem";
generate_cert($cert, "-copy_extensions", "copy",
              "-in", srctop_file(@certs, "ext-check.csr"));
cert_ext_has_n_different_lines($cert, 4, $SKID_AKID); # SKID != AKID
strict_verify($cert, 1);

$cert = "v3_EE_no_AKID.pem";
generate_cert($cert, "-addext", "authorityKeyIdentifier = none",
    "-key", srctop_file(@certs, "ee-key.pem"));
has_SKID($cert, 1);
has_AKID($cert, 0);
strict_verify($cert, 0, $ca_cert);


# Key Usage

$cert = "self-signed_CA_no_keyUsage.pem";
generate_cert($cert, "-in", srctop_file(@certs, "ext-check.csr"));
has_keyUsage($cert, 0);
$cert = "self-signed_CA_with_keyUsages.pem";
generate_cert($cert, "-in", srctop_file(@certs, "ext-check.csr"),
    "-copy_extensions", "copy");
has_keyUsage($cert, 1);

# Generate cert using req with '-modulus'
ok(run(app(["openssl", "req", "-x509", "-new", "-days", "365",
            "-key", srctop_file("test", "testrsa.pem"),
            "-config", srctop_file('test', 'test.cnf'),
            "-out", "testreq-cert.pem",
            "-modulus"])), "cert req creation - with -modulus");

# Verify cert
ok(run(app(["openssl", "x509", "-in", "testreq-cert.pem",
            "-noout", "-text"])), "cert verification");

# Generate cert with explicit start and end dates
my %today = (strftime("%Y-%m-%d", gmtime) => 1);
my $cert = "self-signed_explicit_date.pem";
ok(run(app(["openssl", "req", "-x509", "-new", "-text",
            "-config", srctop_file('test', 'test.cnf'),
            "-key", srctop_file("test", "testrsa.pem"),
            "-not_before", "today",
            "-not_after", "today",
            "-out", $cert]))
&& ++$today{strftime("%Y-%m-%d", gmtime)}
&& (grep { defined $today{$_} } get_not_before_date($cert))
&& (grep { defined $today{$_} } get_not_after_date($cert)), "explicit start and end dates");
