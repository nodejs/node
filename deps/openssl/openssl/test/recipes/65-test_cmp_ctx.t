#! /usr/bin/env perl
# Copyright 2007-2019 The OpenSSL Project Authors. All Rights Reserved.
# Copyright Nokia 2007-2019
# Copyright Siemens AG 2015-2019
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use OpenSSL::Test;              # get 'plan'
use OpenSSL::Test::Simple;
use OpenSSL::Test::Utils;

setup("test_cmp_ctx");

plan skip_all => "This test is not supported in a no-cmp build"
    if disabled("cmp");

simple_test("test_cmp_ctx", "cmp_ctx_test", "cmp_ctx");
