#! /usr/bin/env perl
# Copyright 2017-2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT with srctop_file/;
use OpenSSL::Test::Utils;

setup("test_dgst");

plan tests => 5;

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
