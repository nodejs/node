#! /usr/bin/env perl
# Copyright 2015-2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_ssl_test_ctx");

plan tests => 1;
ok(run(test(["ssl_test_ctx_test", srctop_file("test", "ssl_test_ctx_test.cnf")])),
   "running ssl_test_ctx_test ssl_test_ctx_test.cnf");
