#! /usr/bin/env perl
# Copyright 2021-2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_x509_acert");

plan skip_all => "test_x509_acert uses ec which is not supported by this build"
    if disabled("ec");

plan tests => 1;

ok(run(test(["x509_acert_test", srctop_file("test", "certs", "acert.pem"),
            srctop_file("test", "certs", "acert_ietf.pem"),
            srctop_file("test", "certs", "acert_bc1.pem"),
            srctop_file("test", "certs", "acert_bc2.pem")])),
       "running x509_acert_test");

