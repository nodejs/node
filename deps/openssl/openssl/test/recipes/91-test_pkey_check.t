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
use OpenSSL::Test qw/:DEFAULT data_file/;
use OpenSSL::Test::Utils;

sub check_key {
    my $f = shift;

    return run(app(['openssl', 'pkey', '-check', '-text',
                    '-in', $f]));
}

sub check_key_notok {
    my $f = shift;
    my $str = "$f should fail validation";

    $f = data_file($f);

    if ( -s $f ) {
        ok(!check_key($f), $str);
    } else {
        fail("Missing file $f");
    }
}

setup("test_pkey_check");

my @tests = ();

push(@tests, (
    # For EC keys the range for the secret scalar `k` is `1 <= k <= n-1`
    "ec_p256_bad_0.pem", # `k` set to `n` (equivalent to `0 mod n`, invalid)
    "ec_p256_bad_1.pem", # `k` set to `n+1` (equivalent to `1 mod n`, invalid)
    )) unless disabled("ec");

push(@tests, (
    # For SM2 keys the range for the secret scalar `k` is `1 <= k < n-1`
    "sm2_bad_neg1.pem", # `k` set to `n-1` (invalid, because SM2 range)
    "sm2_bad_0.pem", # `k` set to `n` (equivalent to `0 mod n`, invalid)
    "sm2_bad_1.pem", # `k` set to `n+1` (equivalent to `1 mod n`, invalid)
    )) unless disabled("sm2");

plan skip_all => "No tests within the current enabled feature set"
    unless @tests;

plan tests => scalar(@tests);

foreach my $t (@tests) {
    check_key_notok($t);
}
