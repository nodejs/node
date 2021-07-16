#! /usr/bin/env perl
# Copyright 2007-2021 The OpenSSL Project Authors. All Rights Reserved.
# Copyright Nokia 2007-2020
# Copyright Siemens AG 2015-2020
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT data_file srctop_file srctop_dir bldtop_file bldtop_dir/;
use OpenSSL::Test::Utils;

BEGIN {
    setup("test_cmp_server");
}

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);

plan skip_all => "This test is not supported in a no-cmp build"
    if disabled("cmp");

plan skip_all => "This test is not supported in a no-ec build"
    if disabled("ec");

plan tests => 2 + ($no_fips ? 0 : 1); #fips test

my @basic_cmd = ("cmp_server_test", data_file("CR_protected_PBM_1234.der"));

ok(run(test([@basic_cmd, "none"])));

ok(run(test([@basic_cmd, "default", srctop_file("test", "default.cnf")])));

unless ($no_fips) {
    ok(run(test([@basic_cmd, "fips", srctop_file("test", "fips-and-base.cnf")])));
}
