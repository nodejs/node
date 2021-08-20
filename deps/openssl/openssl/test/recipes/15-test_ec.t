#! /usr/bin/env perl
# Copyright 2015-2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

setup("test_ec");

plan tests => 11;

require_ok(srctop_file('test','recipes','tconversion.pl'));

ok(run(test(["ectest"])), "running ectest");

SKIP: {
    skip "Skipping EC conversion test", 3
        if disabled("ec");

    subtest 'EC conversions -- private key' => sub {
        tconversion("ec", srctop_file("test","testec-p256.pem"));
    };
    subtest 'EC conversions -- private key PKCS#8' => sub {
        tconversion("ec", srctop_file("test","testec-p256.pem"), "pkey");
    };
    subtest 'EC conversions -- public key' => sub {
        tconversion("ec", srctop_file("test","testecpub-p256.pem"),
                    "ec", "-pubin", "-pubout");
    };
}

SKIP: {
    skip "Skipping EdDSA conversion test", 6
        if disabled("ec");

    subtest 'Ed25519 conversions -- private key' => sub {
        tconversion("pkey", srctop_file("test","tested25519.pem"));
    };
    subtest 'Ed25519 conversions -- private key PKCS#8' => sub {
        tconversion("pkey", srctop_file("test","tested25519.pem"), "pkey");
    };
    subtest 'Ed25519 conversions -- public key' => sub {
        tconversion("pkey", srctop_file("test","tested25519pub.pem"),
                    "pkey", "-pubin", "-pubout");
    };

    subtest 'Ed448 conversions -- private key' => sub {
        tconversion("pkey", srctop_file("test","tested448.pem"));
    };
    subtest 'Ed448 conversions -- private key PKCS#8' => sub {
        tconversion("pkey", srctop_file("test","tested448.pem"), "pkey");
    };
    subtest 'Ed448 conversions -- public key' => sub {
        tconversion("pkey", srctop_file("test","tested448pub.pem"),
                    "pkey", "-pubin", "-pubout");
    };
}
