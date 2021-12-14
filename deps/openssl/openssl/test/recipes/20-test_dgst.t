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
use File::Basename;
use OpenSSL::Test qw/:DEFAULT with srctop_file bldtop_file/;
use OpenSSL::Test::Utils;

setup("test_dgst");

plan tests => 10;

sub tsignverify {
    my $testtext = shift;
    my $privkey = shift;
    my $pubkey = shift;

    my $data_to_sign = srctop_file('test', 'data.bin');
    my $other_data = srctop_file('test', 'data2.bin');

    my $sigfile = basename($privkey, '.pem') . '.sig';
    plan tests => 4;

    ok(run(app(['openssl', 'dgst', '-sign', $privkey,
                '-out', $sigfile,
                $data_to_sign])),
       $testtext.": Generating signature");

    ok(run(app(['openssl', 'dgst', '-prverify', $privkey,
                '-signature', $sigfile,
                $data_to_sign])),
       $testtext.": Verify signature with private key");

    ok(run(app(['openssl', 'dgst', '-verify', $pubkey,
                '-signature', $sigfile,
                $data_to_sign])),
       $testtext.": Verify signature with public key");

    ok(!run(app(['openssl', 'dgst', '-verify', $pubkey,
                 '-signature', $sigfile,
                 $other_data])),
       $testtext.": Expect failure verifying mismatching data");
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

subtest "HMAC generation with `dgst` CLI" => sub {
    plan tests => 2;

    my $testdata = srctop_file('test', 'data.bin');
    #HMAC the data twice to check consistency
    my @hmacdata = run(app(['openssl', 'dgst', '-sha256', '-hmac', '123456',
                            $testdata, $testdata]), capture => 1);
    chomp(@hmacdata);
    my $expected = qr/HMAC-SHA2-256\(\Q$testdata\E\)= 6f12484129c4a761747f13d8234a1ff0e074adb34e9e9bf3a155c391b97b9a7c/;
    ok($hmacdata[0] =~ $expected, "HMAC: Check HMAC value is as expected ($hmacdata[0]) vs ($expected)");
    ok($hmacdata[1] =~ $expected,
       "HMAC: Check second HMAC value is consistent with the first ($hmacdata[1]) vs ($expected)");
};

subtest "HMAC generation with `dgst` CLI, default digest" => sub {
    plan tests => 2;

    my $testdata = srctop_file('test', 'data.bin');
    #HMAC the data twice to check consistency
    my @hmacdata = run(app(['openssl', 'dgst', '-hmac', '123456',
                            $testdata, $testdata]), capture => 1);
    chomp(@hmacdata);
    my $expected = qr/HMAC-SHA256\(\Q$testdata\E\)= 6f12484129c4a761747f13d8234a1ff0e074adb34e9e9bf3a155c391b97b9a7c/;
    ok($hmacdata[0] =~ $expected, "HMAC: Check HMAC value is as expected ($hmacdata[0]) vs ($expected)");
    ok($hmacdata[1] =~ $expected,
       "HMAC: Check second HMAC value is consistent with the first ($hmacdata[1]) vs ($expected)");
};

subtest "HMAC generation with `dgst` CLI, key via option" => sub {
    plan tests => 2;

    my $testdata = srctop_file('test', 'data.bin');
    #HMAC the data twice to check consistency
    my @hmacdata = run(app(['openssl', 'dgst', '-sha256', '-hmac',
                            '-macopt', 'hexkey:FFFF',
                            $testdata, $testdata]), capture => 1);
    chomp(@hmacdata);
    my $expected = qr/HMAC-SHA2-256\(\Q$testdata\E\)= b6727b7bb251dfa65846e0a8223bdd57d244aa6d7e312cb906d8e21f2dee3a57/;
    ok($hmacdata[0] =~ $expected, "HMAC: Check HMAC value is as expected ($hmacdata[0]) vs ($expected)");
    ok($hmacdata[1] =~ $expected,
       "HMAC: Check second HMAC value is consistent with the first ($hmacdata[1]) vs ($expected)");
};

subtest "Custom length XOF digest generation with `dgst` CLI" => sub {
    plan tests => 2;

    my $testdata = srctop_file('test', 'data.bin');
    #Digest the data twice to check consistency
    my @xofdata = run(app(['openssl', 'dgst', '-shake128', '-xoflen', '64',
                           $testdata, $testdata]), capture => 1);
    chomp(@xofdata);
    my $expected = qr/SHAKE-128\(\Q$testdata\E\)= bb565dac72640109e1c926ef441d3fa64ffd0b3e2bf8cd73d5182dfba19b6a8a2eab96d2df854b647b3795ef090582abe41ba4e0717dc4df40bc4e17d88e4677/;
    ok($xofdata[0] =~ $expected, "XOF: Check digest value is as expected ($xofdata[0]) vs ($expected)");
    ok($xofdata[1] =~ $expected,
       "XOF: Check second digest value is consistent with the first ($xofdata[1]) vs ($expected)");
};
