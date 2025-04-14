#! /usr/bin/env perl
# Copyright 2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test::Simple;
use OpenSSL::Test qw/:DEFAULT srctop_dir/;

setup("test_evp_pkey_provided");

plan tests => 1;

ok(run(test(["evp_pkey_provided_test",
            srctop_dir("test", "recipes", "30-test_evp_pkey_provided")])),
   "running evp_pkey_provided_test");
