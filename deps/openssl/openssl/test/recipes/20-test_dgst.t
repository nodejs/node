#! /usr/bin/env perl
# Copyright 2017-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT with srctop_file bldtop_file/;
use OpenSSL::Test::Utils;

setup("test_dgst");

plan tests => 6;

sub tsignverify {
    my $testtext = shift;
    my $privkey = shift;
    my $pubkey = shift;

    my $data_to_sign = srctop_file('test', 'README');
    my $other_data = srctop_file('test', 'README.external');

    plan tests => 4;

    ok(run(app(['openssl', 'dgst', '-sign', $privkey,
                '-out', 'testdgst.sig',
                $data_to_sign])),
       $testtext.": Generating signature");

    ok(run(app(['openssl', 'dgst', '-prverify', $privkey,
                '-signature', 'testdgst.sig',
                $data_to_sign])),
       $testtext.": Verify signature with private key");

    ok(run(app(['openssl', 'dgst', '-verify', $pubkey,
                '-signature', 'testdgst.sig',
                $data_to_sign])),
       $testtext.": Verify signature with public key");

    ok(!run(app(['openssl', 'dgst', '-verify', $pubkey,
                 '-signature', 'testdgst.sig',
                 $other_data])),
       $testtext.": Expect failure verifying mismatching data");

    unlink 'testdgst.sig';
}

SKIP: {
    skip "RSA is not supported by this OpenSSL build", 1
        if disabled("rsa");

    subtest "RSA signature generation and verification with `dgst` CLI" => sub {
        tsignverify("RSA",
                    srctop_file("test","testrsa.pem"),
                    srctop_file("test","testrsapub.pem"));
    };
}

SKIP: {
    skip "DSA is not supported by this OpenSSL build", 1
        if disabled("dsa");

    subtest "DSA signature generation and verification with `dgst` CLI" => sub {
        tsignverify("DSA",
                    srctop_file("test","testdsa.pem"),
                    srctop_file("test","testdsapub.pem"));
    };
}

SKIP: {
    skip "ECDSA is not supported by this OpenSSL build", 1
        if disabled("ec");

    subtest "ECDSA signature generation and verification with `dgst` CLI" => sub {
        tsignverify("ECDSA",
                    srctop_file("test","testec-p256.pem"),
                    srctop_file("test","testecpub-p256.pem"));
    };
}

SKIP: {
    skip "EdDSA is not supported by this OpenSSL build", 2
        if disabled("ec");

    skip "EdDSA is not supported with `dgst` CLI", 2;

    subtest "Ed25519 signature generation and verification with `dgst` CLI" => sub {
        tsignverify("Ed25519",
                    srctop_file("test","tested25519.pem"),
                    srctop_file("test","tested25519pub.pem"));
    };

    subtest "Ed448 signature generation and verification with `dgst` CLI" => sub {
        tsignverify("Ed448",
                    srctop_file("test","tested448.pem"),
                    srctop_file("test","tested448pub.pem"));
    };
}

SKIP: {
    skip "dgst with engine is not supported by this OpenSSL build", 1
        if disabled("engine") || disabled("dynamic-engine");

    subtest "SHA1 generation by engine with `dgst` CLI" => sub {
        plan tests => 1;

        my $testdata = srctop_file('test', 'data.bin');
        # intentionally using -engine twice, please do not remove the duplicate line
        my @macdata = run(app(['openssl', 'dgst', '-sha1',
                               '-engine', $^O eq 'linux' ? bldtop_file("engines", "ossltest.so") : "ossltest",
                               '-engine', $^O eq 'linux' ? bldtop_file("engines", "ossltest.so") : "ossltest",
                               $testdata]), capture => 1);
        chomp(@macdata);
        my $expected = qr/SHA1\(\Q$testdata\E\)= 000102030405060708090a0b0c0d0e0f10111213/;
        ok($macdata[0] =~ $expected, "SHA1: Check HASH value is as expected ($macdata[0]) vs ($expected)");
    }
}
