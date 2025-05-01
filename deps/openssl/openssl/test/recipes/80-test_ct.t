#! /usr/bin/env perl
# Copyright 2015-2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test qw/:DEFAULT srctop_file srctop_dir/;
use OpenSSL::Test::Simple;

setup("test_ct");

$ENV{CT_DIR} = srctop_dir("test", "ct");
$ENV{CERTS_DIR} = srctop_dir("test", "certs");
simple_test("test_ct", "ct_test", "ct", "ec");
