#! /usr/bin/env perl
# Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test;
use OpenSSL::Test::Utils;

my $test_name = "test_dtls_mtu";
setup($test_name);

plan skip_all => "$test_name needs DTLS and PSK support enabled"
    if disabled("dtls1_2") || disabled("psk");

plan tests => 1;

ok(run(test(["dtls_mtu_test"])), "running dtls_mtu_test");
