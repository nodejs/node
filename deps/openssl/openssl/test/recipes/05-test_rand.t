#! /usr/bin/env perl
# Copyright 2015-2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;
use OpenSSL::Test;
use OpenSSL::Test::Utils;

plan tests => 5;
setup("test_rand");

ok(run(test(["rand_test"])));
ok(run(test(["drbgtest"])));
ok(run(test(["rand_status_test"])));

SKIP: {
    skip "engine is not supported by this OpenSSL build", 2
        if disabled("engine") || disabled("dynamic-engine");

    my $success;
    my @randdata;
    my $expected = '0102030405060708090a0b0c0d0e0f10';

    @randdata = run(app(['openssl', 'rand', '-engine', 'ossltest', '-hex', '16' ]),
                    capture => 1, statusvar => \$success);
    chomp(@randdata);
    ok($success && $randdata[0] eq $expected,
       "rand with ossltest: Check rand output is as expected");

    @randdata = run(app(['openssl', 'rand', '-engine', 'dasync', '-hex', '16' ]),
                    capture => 1, statusvar => \$success);
    chomp(@randdata);
    ok($success && length($randdata[0]) == 32,
       "rand with dasync: Check rand output is of expected length");
}
