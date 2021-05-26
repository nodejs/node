#! /usr/bin/env perl
# Copyright 2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

setup("test_http");

plan tests => 1;

SKIP: {
    skip "sockets disabled", 1 if disabled("sock");
    ok(run(test(["http_test",
                 srctop_file("test", "certs", "ca-cert.pem")])));
}
