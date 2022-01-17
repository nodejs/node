#! /usr/bin/env perl
# Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw(:DEFAULT bldtop_dir srctop_dir srctop_file bldtop_file);
use OpenSSL::Test::Utils;

BEGIN {
setup("test_acvp");
}

my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);

plan skip_all => "ACVP is not supported by this test"
    if $no_fips || disabled("acvp-tests");

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

plan tests => 1;

ok(run(test(["acvp_test", "-config", srctop_file("test","fips.cnf")])),
   "running acvp_test");
