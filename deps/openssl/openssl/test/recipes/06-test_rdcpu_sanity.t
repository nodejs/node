#! /usr/bin/env perl

# Copyright 2018-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;

use OpenSSL::Test;              # get 'plan'
use OpenSSL::Test::Simple;
use OpenSSL::Test::Utils;

setup("test_rdcpu_sanity");

# We also need static builds to be enabled even on linux
plan skip_all => "This test is unsupported if static builds are not enabled"
    if disabled("static");

simple_test("test_rdcpu_sanity", "rdcpu_sanitytest");
