#! /usr/bin/env perl
# Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test qw(:DEFAULT data_file bldtop_dir srctop_file srctop_dir bldtop_file);
use OpenSSL::Test::Utils;

BEGIN {
setup("test_provider_status");
}

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);

plan tests => 5;

ok(run(test(["provider_status_test", "-provider_name", "null"])),
   "null provider test");

ok(run(test(["provider_status_test", "-provider_name", "base"])),
   "base provider test");

ok(run(test(["provider_status_test", "-provider_name", "default"])),
   "default provider test");

SKIP: {
    skip "Skipping legacy test", 1
        if disabled("legacy");
    ok(run(test(["provider_status_test", "-provider_name", "legacy"])),
       "legacy provider test");
}

SKIP: {
    skip "Skipping fips test", 1
        if $no_fips;
    ok(run(test(["provider_status_test", "-config", srctop_file("test","fips.cnf"),
                 "-provider_name", "fips"])),
       "fips provider test");
}
