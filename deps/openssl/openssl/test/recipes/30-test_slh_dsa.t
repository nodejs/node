#! /usr/bin/env perl
# Copyright 2024-2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw(:DEFAULT srctop_dir bldtop_dir srctop_file);
use OpenSSL::Test::Utils;

BEGIN {
    setup("test_slh_dsa");
}

my $provconf = srctop_file("test", "fips-and-base.cnf");
my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

plan skip_all => 'SLH-DSA is not supported in this build' if disabled('slh-dsa');
plan tests => 2;

ok(run(test(["slh_dsa_test"])), "running slh_dsa_test");

SKIP: {
    skip "Skipping FIPS tests", 1
        if $no_fips;

    # SLH-DSA is only present after OpenSSL 3.5
    run(test(["fips_version_test", "-config", $provconf, ">=3.5.0"]),
             capture => 1, statusvar => \my $exit);
    skip "FIPS provider version is too old for SLH_DSA test", 1
        if !$exit;

    ok(run(test(["slh_dsa_test", "-config",  $provconf])),
           "running slh_dsa_test with FIPS");
}
