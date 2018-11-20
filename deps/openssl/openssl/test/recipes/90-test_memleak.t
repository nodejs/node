#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test;

setup("test_memleak");
plan tests => 2;
ok(run(test(["memleaktest"])), "running leak test");
ok(run(test(["memleaktest", "freeit"])), "running no leak test");
