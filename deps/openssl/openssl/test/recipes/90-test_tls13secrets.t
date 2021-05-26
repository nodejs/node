#! /usr/bin/env perl
# Copyright 2016-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test;
use OpenSSL::Test::Utils;

my $test_name = "test_tls13secrets";
setup($test_name);

plan skip_all => "$test_name is not supported in this build"
    if disabled("tls1_3")
       || disabled("shared")
       || (disabled("ec") && disabled("dh"));

plan tests => 1;

ok(run(test(["tls13secretstest"])), "running tls13secretstest");
