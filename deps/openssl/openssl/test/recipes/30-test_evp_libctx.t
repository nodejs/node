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
    setup("test_evp_libctx");
}

my $no_legacy = disabled('legacy') || ($ENV{NO_LEGACY} // 0);
my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

# If no fips then run the test with no extra arguments.
my @test_args = ( );

plan tests => ($no_fips ? 0 : 1) + ($no_legacy ? 0 : 1) + 1;

unless ($no_fips) {
    @test_args = ("-config", srctop_file("test","fips-and-base.cnf"),
                  "-provider", "fips");

    ok(run(test(["evp_libctx_test", @test_args])), "running fips evp_libctx_test");
}

ok(run(test(["evp_libctx_test",
             "-config", srctop_file("test","default.cnf"),])),
   "running default evp_libctx_test");

unless ($no_legacy) {
    ok(run(test(["evp_libctx_test",
                 "-config", srctop_file("test","default-and-legacy.cnf"),])),
       "running default-and-legacy evp_libctx_test");
}

