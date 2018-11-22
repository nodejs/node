#! /usr/bin/perl

# Copyright 2018 The OpenSSL Project Authors. All Rights Reserved.
# 
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;

use OpenSSL::Test;              # get 'plan'
use OpenSSL::Test::Simple;
use OpenSSL::Test::Utils;

setup("test_rdrand_sanity");

# We also need static builds to be enabled even on linux
plan skip_all => "This test is unsupported if static builds are not enabled"
    if disabled("static");

simple_test("test_rdrand_sanity", "rdrand_sanitytest");
