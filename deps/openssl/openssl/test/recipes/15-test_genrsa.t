#! /usr/bin/env perl
# Copyright 2017-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT srctop_file srctop_dir bldtop_dir bldtop_file/;
use OpenSSL::Test::Utils;

BEGIN {
    setup("test_genrsa");
}

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);

plan tests =>
    ($no_fips ? 0 : 5)          # Extra FIPS related tests
    + 15;

# We want to know that an absurdly small number of bits isn't support
is(run(app([ 'openssl', 'genpkey', '-out', 'genrsatest.pem',
             '-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:8',
             '-pkeyopt', 'rsa_keygen_pubexp:3'])),
           0, "genpkey 8");
is(run(app([ 'openssl', 'genrsa', '-3', '-out', 'genrsatest.pem', '8'])),
           0, "genrsa -3 8");

# Depending on the shared library, we might have different lower limits.
# Let's find it!  This is a simple binary search
# ------------------------------------------------------------
# NOTE: $good may need an update in the future
# ------------------------------------------------------------
note "Looking for lowest amount of bits";
my $bad = 3;                    # Log2 of number of bits (2 << 3  == 8)
my $good = 11;                  # Log2 of number of bits (2 << 11 == 2048)
my $fin;
while ($good > $bad + 1) {
    my $checked = int(($good + $bad + 1) / 2);
    my $bits = 2 ** $checked;
    $fin = run(app([ 'openssl', 'genpkey', '-out', 'genrsatest.pem',
                     '-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_pubexp:65537',
                     '-pkeyopt', "rsa_keygen_bits:$bits",
                   ], stderr => undef));
    if ($fin) {
        note 2 ** $checked, " bits is good";
        $good = $checked;
    } else {
        note 2 ** $checked, " bits is bad";
        $bad = $checked;
    }
}
$good++ if $good == $bad;
$good = 2 ** $good;
note "Found lowest allowed amount of bits to be $good";

ok(run(app([ 'openssl', 'genpkey', '-algorithm', 'RSA',
             '-pkeyopt', 'rsa_keygen_pubexp:65537',
             '-pkeyopt', "rsa_keygen_bits:$good",
             '-out', 'genrsatest.pem' ])),
   "genpkey $good");
ok(run(app([ 'openssl', 'pkey', '-check', '-in', 'genrsatest.pem', '-noout' ])),
   "pkey -check");

ok(run(app([ 'openssl', 'genpkey', '-algorithm', 'RSA',
             '-pkeyopt', 'rsa_keygen_bits:2048',
             '-out', 'genrsatest2048.pem' ])),
   "genpkey 2048 bits");
ok(run(app([ 'openssl', 'pkey', '-check', '-in', 'genrsatest2048.pem', '-noout' ])),
   "pkey -check");

ok(!run(app([ 'openssl', 'genpkey', '-algorithm', 'RSA',
             '-pkeyopt', 'hexe:02',
             '-out', 'genrsatest.pem' ])),
   "genpkey with a bad public exponent should fail");
ok(!run(app([ 'openssl', 'genpkey', '-algorithm', 'RSA',
             '-pkeyopt', 'e:65538',
             '-out', 'genrsatest.pem' ])),
   "genpkey with a even public exponent should fail");
ok(!run(app([ 'openssl', 'genpkey', '-propquery', 'unknown',
             '-algorithm', 'RSA' ])),
   "genpkey requesting unknown=yes property should fail");

 SKIP: {
    skip "Skipping rsa command line test", 2 if disabled("deprecated-3.0");

    ok(run(app([ 'openssl', 'genrsa', '-3', '-out', 'genrsatest.pem', $good ])),
       "genrsa -3 $good");
    ok(run(app([ 'openssl', 'rsa', '-check', '-in', 'genrsatest.pem', '-noout' ])),
       "rsa -check");
 }

ok(run(app([ 'openssl', 'genrsa', '-f4', '-out', 'genrsatest.pem', $good ])),
   "genrsa -f4 $good");
ok(run(app([ 'openssl', 'rsa', '-check', '-in', 'genrsatest.pem', '-noout' ])),
   "rsa -check");
ok(run(app([ 'openssl', 'rsa', '-in', 'genrsatest.pem', '-out', 'genrsatest-enc.pem',
   '-aes256', '-passout', 'pass:x' ])),
   "rsa encrypt");
ok(run(app([ 'openssl', 'rsa', '-in', 'genrsatest-enc.pem', '-passin', 'pass:x' ])),
   "rsa decrypt");

unless ($no_fips) {
    my $provconf = srctop_file("test", "fips-and-base.cnf");
    my $provpath = bldtop_dir("providers");
    my @prov = ( "-provider-path", $provpath,
                 "-config", $provconf);

    $ENV{OPENSSL_TEST_LIBCTX} = "1";
    ok(run(app(['openssl', 'genpkey',
                @prov,
                '-algorithm', 'RSA',
                '-pkeyopt', 'bits:2080',
                '-out', 'genrsatest2080.pem'])),
       "Generating RSA key with > 2048 bits and < 3072 bits");
    ok(run(app(['openssl', 'genpkey',
                @prov,
                '-algorithm', 'RSA',
                '-pkeyopt', 'bits:3072',
                '-out', 'genrsatest3072.pem'])),
       "Generating RSA key with 3072 bits");

   ok(!run(app(['openssl', 'genrsa', @prov, '512'])),
       "Generating RSA key with 512 bits should fail in FIPS provider");

   ok(!run(app(['openssl', 'genrsa',
                @prov,
                '-provider', 'default',
                '-propquery', '?fips!=yes',
                '512'])),
       "Generating RSA key with 512 bits should succeed with FIPS provider as".
       " default with a non-FIPS property query");

    # We want to know that an absurdly large number of bits fails the RNG check
    is(run(app([ 'openssl', 'genpkey',
                 @prov,
                 '-algorithm', 'RSA',
                 '-pkeyopt', 'bits:1000000000',
                 '-out', 'genrsatest.pem'])),
               0, "genpkey 1000000000");
}
