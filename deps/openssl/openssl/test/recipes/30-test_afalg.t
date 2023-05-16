#! /usr/bin/env perl
# Copyright 2015-2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT/;
use OpenSSL::Test::Utils;

my $test_name = "test_afalg";
setup($test_name);

plan skip_all => "$test_name not supported for this build"
    if disabled("afalgeng");

plan tests => 1;

ok(run(test(["afalgtest"])), "running afalgtest");
