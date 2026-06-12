#! /usr/bin/env perl
# Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw(:DEFAULT srctop_file srctop_dir bldtop_dir);
use OpenSSL::Test::Utils;

BEGIN {
    setup("test_lms");
}

plan skip_all => 'LMS is not supported in this build' if disabled('lms');

my $provconf = srctop_file("test", "fips-and-base.cnf");
my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

plan skip_all => 'LMS is not supported in this build' if disabled('lms');
plan tests => 2;

ok(run(test(["lms_test"])), "running lms_test");

SKIP: {
    skip "Skipping FIPS tests", 1
        if $no_fips;

    # LMS is only present after OpenSSL 3.6
    run(test(["fips_version_test", "-config", $provconf, ">=3.6.0"]),
             capture => 1, statusvar => \my $exit);
    skip "FIPS provider version is too old for LMS test", 1
        if !$exit;

    ok(run(test(["lms_test", "-config",  $provconf])),
       "running lms_test with fips");
}
