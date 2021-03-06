#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_verify_extra");

plan tests => 1;

ok(run(test(["verify_extra_test",
             srctop_file("test", "certs", "roots.pem"),
             srctop_file("test", "certs", "untrusted.pem"),
             srctop_file("test", "certs", "bad.pem"),
             srctop_file("test", "certs", "rootCA.pem")])));
