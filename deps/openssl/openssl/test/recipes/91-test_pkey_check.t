#! /usr/bin/env perl
# Copyright 2017-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT data_file/;
use OpenSSL::Test::Utils;

sub pkey_check {
    my $f = shift;

    return run(app(['openssl', 'pkey', '-check', '-text',
                    '-in', $f]));
}

sub check_key {
    my $f = shift;
    my $should_fail = shift;
    my $str;


    $str = "$f should fail validation" if $should_fail;
    $str = "$f should pass validation" unless $should_fail;

    $f = data_file($f);

    if ( -s $f ) {
        if ($should_fail) {
            ok(!pkey_check($f), $str);
        } else {
            ok(pkey_check($f), $str);
        }
    } else {
        fail("Missing file $f");
    }
}

setup("test_pkey_check");

my @negative_tests = ();

push(@negative_tests, (
    # For EC keys the range for the secret scalar `k` is `1 <= k <= n-1`
    "ec_p256_bad_0.pem", # `k` set to `n` (equivalent to `0 mod n`, invalid)
    "ec_p256_bad_1.pem", # `k` set to `n+1` (equivalent to `1 mod n`, invalid)
    )) unless disabled("ec");

push(@negative_tests, (
    # For SM2 keys the range for the secret scalar `k` is `1 <= k < n-1`
    "sm2_bad_neg1.pem", # `k` set to `n-1` (invalid, because SM2 range)
    "sm2_bad_0.pem", # `k` set to `n` (equivalent to `0 mod n`, invalid)
    "sm2_bad_1.pem", # `k` set to `n+1` (equivalent to `1 mod n`, invalid)
    )) unless disabled("sm2");

my @positive_tests = ();

push(@positive_tests, (
    "dhpkey.pem"
    )) unless disabled("dh");

plan skip_all => "No tests within the current enabled feature set"
    unless @negative_tests && @positive_tests;

plan tests => scalar(@negative_tests) + scalar(@positive_tests);

foreach my $t (@negative_tests) {
    check_key($t, 1);
}

foreach my $t (@positive_tests) {
    check_key($t, 0);
}
