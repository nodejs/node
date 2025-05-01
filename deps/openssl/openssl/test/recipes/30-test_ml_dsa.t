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
    setup("test_ml_dsa");
}

my $provconf = srctop_file("test", "fips-and-base.cnf");
# fips will be added later
my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

plan skip_all => 'ML-DSA is not supported in this build' if disabled('ml-dsa');
plan tests => 12;

require_ok(srctop_file('test','recipes','tconversion.pl'));

subtest "ml-dsa-44 conversions using 'openssl pkey' -- private key" => sub {
    tconversion( -type => "pkey",
                 -in => srctop_file("test","testmldsa44.pem"),
                 -prefix => "mldsa44-pkey-priv" );
};
subtest "ml-dsa-44 conversions using 'openssl pkey' -- pkcs8 key" => sub {
    tconversion( -type => "pkey",
                 -in => srctop_file("test","testmldsa44.pem"),
                 -args => ["pkey"],
                 -prefix => "mldsa44-pkey-pkcs8" );
};
subtest "ml-dsa-44 conversions using 'openssl pkey' -- pub key" => sub {
    tconversion( -type => "pkey",
                 -in => srctop_file("test","testmldsa44pub.pem"),
                 -args => ["pkey", "-pubin", "-pubout"],
                 -prefix => "mldsa44-pkey-pub" );
};

subtest "ml-dsa-65 conversions using 'openssl pkey' -- private key" => sub {
    tconversion( -type => "pkey",
                 -in => srctop_file("test","testmldsa65.pem"),
                 -prefix => "mldsa65-pkey-priv");
};
subtest "ml-dsa-65 conversions using 'openssl pkey' -- pkcs8 key" => sub {
    tconversion( -type => "pkey", -in => srctop_file("test","testmldsa65.pem"),
                 -args => ["pkey"], -prefix => "mldsa65-pkey-pkcs8");
};
subtest "ml-dsa-65 conversions using 'openssl pkey' -- pub key" => sub {
    tconversion( -type => "pkey",
                 -in => srctop_file("test","testmldsa65pub.pem"),
                 -args => ["pkey", "-pubin", "-pubout"],
                 -prefix => "mldsa65-pkey-pub" );
};

subtest "ml-dsa-87 conversions using 'openssl pkey' -- private key" => sub {
    tconversion( -type => "pkey",
                 -in => srctop_file("test","testmldsa87.pem"),
                 -prefix => "mldsa87-pkey-priv" );
};
subtest "ml-dsa-87 conversions using 'openssl pkey' -- pkcs8 key" => sub {
    tconversion( -type => "pkey",
                 -in => srctop_file("test","testmldsa87.pem"),
                 -args => ["pkey"],
                 -prefix => "mldsa87-pkey-pkcs8" );
};
subtest "ml-dsa-87 conversions using 'openssl pkey' -- pub key" => sub {
    tconversion( -type => "pkey",
                 -in => srctop_file("test","testmldsa87pub.pem"),
                 -args => ["pkey", "-pubin", "-pubout"],
                 -prefix => "mldsa87-pkey-pub" );
};

ok(run(test(["ml_dsa_test"])), "running ml_dsa_test");

SKIP: {
    skip "Skipping FIPS tests", 1
        if $no_fips;

    # ML-DSA is only present after OpenSSL 3.5
    run(test(["fips_version_test", "-config", $provconf, ">=3.5.0"]),
             capture => 1, statusvar => \my $exit);
    skip "FIPS provider version is too old for ML-DSA test", 1
        if !$exit;

    ok(run(test(["ml_dsa_test", "-config",  $provconf])),
           "running ml_dsa_test with FIPS");
}
