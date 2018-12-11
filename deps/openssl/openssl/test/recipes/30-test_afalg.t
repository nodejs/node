#! /usr/bin/env perl
# Copyright 2015-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT bldtop_dir/;
use OpenSSL::Test::Utils;

my $test_name = "test_afalg";
setup($test_name);

plan skip_all => "$test_name not supported for this build"
    if disabled("afalgeng");

plan tests => 1;

$ENV{OPENSSL_ENGINES} = bldtop_dir("engines");

ok(run(test(["afalgtest"])), "running afalgtest");
