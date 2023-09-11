#! /usr/bin/env perl
# Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use File::Spec::Functions;
use OpenSSL::Test::Simple;
use OpenSSL::Test qw/:DEFAULT srctop_dir data_dir/;

setup("test_ossl_store");

plan tests => 1;

ok(run(test(["ossl_store_test", "-dir", srctop_dir("test"),
             "-in", "testrsa.pem", "-sm2", canonpath("certs/sm2-root.crt"),
             "-data", data_dir()])));
