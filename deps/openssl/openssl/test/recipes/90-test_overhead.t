#! /usr/bin/env perl
# Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test;
use OpenSSL::Test::Utils;

setup("test_overhead");

plan skip_all => "Only supported in no-shared builds"
    if !disabled("shared");

plan tests => 1;

ok(run(test(["cipher_overhead_test"])), "running cipher_overhead_test");
