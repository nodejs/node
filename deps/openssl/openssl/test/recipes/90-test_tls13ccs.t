#! /usr/bin/env perl
# Copyright 2017-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

my $test_name = "test_tls13ccs";
setup($test_name);

plan skip_all => "$test_name is not supported in this build"
    if disabled("tls1_3") || (disabled("ec") && disabled("dh"));

plan tests => 1;

ok(run(test(["tls13ccstest", srctop_file("apps", "server.pem"),
             srctop_file("apps", "server.pem")])), "tls13ccstest");
