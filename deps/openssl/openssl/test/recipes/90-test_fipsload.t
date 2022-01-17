#! /usr/bin/env perl
# Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test qw/:DEFAULT srctop_dir bldtop_dir bldtop_file/;
use OpenSSL::Test::Utils;

BEGIN {
    setup('test_shlibload');
}
use lib srctop_dir('Configurations');
use lib bldtop_dir('.');
use platform;

plan skip_all => 'Test is disabled with disabled fips' if disabled('fips');
plan skip_all => 'Test only supported in a shared build' if disabled('shared');
plan skip_all => 'Test is disabled on AIX' if config('target') =~ m|^aix|;
plan skip_all => 'Test is disabled on NonStop ia64' if config('target') =~ m|^nonstop-nse|;
plan skip_all => 'Test only supported in a dso build' if disabled('dso');
plan skip_all => 'Test is disabled in an address sanitizer build' unless disabled('asan');

plan tests => 1;

my $fips = bldtop_file('providers', platform->dso('fips'));

ok(run(test(['moduleloadtest', $fips, 'OSSL_provider_init'])),
   "trying to load $fips in its own");
