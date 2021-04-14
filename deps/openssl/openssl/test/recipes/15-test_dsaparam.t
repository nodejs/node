#! /usr/bin/env perl
# Copyright 2017-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use File::Spec;
use OpenSSL::Glob;
use OpenSSL::Test qw/:DEFAULT data_file/;
use OpenSSL::Test::Utils;

setup("test_dsaparam");

=pod Generation script

#!/bin/sh

TESTDIR=test/recipes/15-test_dsaparam_data/valid
rm -rf $TESTDIR
mkdir -p $TESTDIR

./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:1024 -pkeyopt qbits:160 -pkeyopt type:fips186_4 -out $TESTDIR/p1024_q160_t1864.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:2048 -pkeyopt qbits:224 -pkeyopt type:fips186_4 -out $TESTDIR/p2048_q224_t1864.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:2048 -pkeyopt qbits:256 -pkeyopt type:fips186_4 -out $TESTDIR/p2048_q256_t1864.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:3072 -pkeyopt qbits:256 -pkeyopt type:fips186_4 -out $TESTDIR/p3072_q256_t1864.pem

./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:1024 -pkeyopt qbits:160 -pkeyopt type:fips186_4 -pkeyopt gindex:1 -out $TESTDIR/p1024_q160_t1864_gind1.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:2048 -pkeyopt qbits:224 -pkeyopt type:fips186_4 -pkeyopt gindex:1 -out $TESTDIR/p2048_q224_t1864_gind1.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:2048 -pkeyopt qbits:256 -pkeyopt type:fips186_4 -pkeyopt gindex:1 -out $TESTDIR/p2048_q256_t1864_gind1.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:3072 -pkeyopt qbits:256 -pkeyopt type:fips186_4 -pkeyopt gindex:1 -out $TESTDIR/p3072_q256_t1864_gind1.pem

./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:1024 -pkeyopt qbits:160 -pkeyopt type:fips186_2 -out $TESTDIR/p1024_q160_t1862.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:1024 -pkeyopt qbits:224 -pkeyopt type:fips186_2 -out $TESTDIR/p1024_q224_t1862.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:1024 -pkeyopt qbits:256 -pkeyopt type:fips186_2 -out $TESTDIR/p1024_q256_t1862.pem

./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:2048 -pkeyopt qbits:160 -pkeyopt type:fips186_2 -out $TESTDIR/p2048_q160_t1862.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:2048 -pkeyopt qbits:224 -pkeyopt type:fips186_2 -out $TESTDIR/p2048_q224_t1862.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:2048 -pkeyopt qbits:256 -pkeyopt type:fips186_2 -out $TESTDIR/p2048_q256_t1862.pem

./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:3072 -pkeyopt qbits:160 -pkeyopt type:fips186_2 -out $TESTDIR/p3072_q160_t1862.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:3072 -pkeyopt qbits:224 -pkeyopt type:fips186_2 -out $TESTDIR/p3072_q224_t1862.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:3072 -pkeyopt qbits:256 -pkeyopt type:fips186_2 -out $TESTDIR/p3072_q256_t1862.pem

./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:1024 -pkeyopt qbits:160 -pkeyopt type:fips186_2 -pkeyopt gindex:1 -out $TESTDIR/p1024_q160_t1862_gind1.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:1024 -pkeyopt qbits:224 -pkeyopt type:fips186_2 -pkeyopt gindex:1 -out $TESTDIR/p1024_q224_t1862_gind1.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:1024 -pkeyopt qbits:256 -pkeyopt type:fips186_2 -pkeyopt gindex:1 -out $TESTDIR/p1024_q256_t1862_gind1.pem

./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:2048 -pkeyopt qbits:160 -pkeyopt type:fips186_2 -pkeyopt gindex:1 -out $TESTDIR/p2048_q160_t1862_gind1.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:2048 -pkeyopt qbits:224 -pkeyopt type:fips186_2 -pkeyopt gindex:1 -out $TESTDIR/p2048_q224_t1862_gind1.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:2048 -pkeyopt qbits:256 -pkeyopt type:fips186_2 -pkeyopt gindex:1 -out $TESTDIR/p2048_q256_t1862_gind1.pem

./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:3072 -pkeyopt qbits:160 -pkeyopt type:fips186_2 -pkeyopt gindex:1 -out $TESTDIR/p3072_q160_t1862_gind1.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:3072 -pkeyopt qbits:224 -pkeyopt type:fips186_2 -pkeyopt gindex:1 -out $TESTDIR/p3072_q224_t1862_gind1.pem
./util/opensslwrap.sh genpkey -genparam -algorithm DSA -pkeyopt dsa_paramgen_bits:3072 -pkeyopt qbits:256 -pkeyopt type:fips186_2 -pkeyopt gindex:1 -out $TESTDIR/p3072_q256_t1862_gind1.pem

=cut

plan skip_all => "DSA isn't supported in this build"
    if disabled("dsa");

my @valid = glob(data_file("valid", "*.pem"));
my @invalid = glob(data_file("invalid", "*.pem"));

my $num_tests = scalar @valid + scalar @invalid;
plan tests => $num_tests;

foreach (@valid) {
    ok(run(app([qw{openssl pkeyparam -noout -check -in}, $_])));
}

foreach (@invalid) {
    ok(!run(app([qw{openssl pkeyparam -noout -check -in}, $_])));
}
