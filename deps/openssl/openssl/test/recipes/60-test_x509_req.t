#! /usr/bin/env perl
# Copyright 2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_dir/;

BEGIN {
    setup("test_x509_req");
}

plan tests => 1;

ok(run(test(["x509_req_test", srctop_dir("test", "certs")])), "running x509_req_test");
