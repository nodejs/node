#! /usr/bin/env perl
# Copyright 2019-2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test;
use OpenSSL::Test::Utils;

setup("test_rand_config");

my @rand_tests = (
    { drbg => 'HASH-DRBG',
      digest => 'SHA2-512/256',
      properties => '',
      expected => ["HASH-DRBG", "digest: 'SHA2-512/256'"],
      desc => 'HASH-DRBG SHA2-512/256' },

    { drbg => 'HASH-DRBG',
      digest => 'SHA3-256',
      properties => '',
      expected => ["HASH-DRBG", "digest: 'SHA3-512'"],
      desc => 'HASH-DRBG SHA3/512' },

    { drbg => 'HMAC-DRBG',
      digest => 'SHA3-256',
      properties => '',
      expected => ["HMAC-DRBG", "mac: HMAC", "digest: 'SHA3-256'"],
      desc => 'HMAC-DRBG SHA3/256' },

    { cipher => 'AES-128-CTR',
      expected => ["CTR-DRBG", "cipher: 'AES-128-CTR'"],
      desc => 'CTR-DRBG AES-128 no DRBG' },
    { expected => ["CTR-DRBG", "cipher: 'AES-256-CTR'"],
      desc => 'CTR-DRBG AES-256 defaults' },
);

my @aria_tests = (
    { drbg => 'CTR-DRBG',
      cipher => 'ARIA-128-CTR',
      properties => '',
      expected => ["CTR-DRBG", "cipher: 'ARIA-128-CTR'"],
      desc => 'CTR-DRBG ARIA-128' },

    { drbg => 'CTR-DRBG',
      cipher => 'ARIA-128-CTR',
      properties => '',
      expected => ["CTR-DRBG", "cipher: 'ARIA-128-CTR'"],
      desc => 'CTR-DRBG ARIA-256' },
);

push @rand_tests, @aria_tests unless disabled("aria");

plan tests => scalar @rand_tests;

my $contents =<<'CONFIGEND';
openssl_conf = openssl_init

[openssl_init]
random = random_section

[random_section]
CONFIGEND

foreach (@rand_tests) {
    my $tmpfile = 'rand_config.cfg';
    open(my $cfg, '>', $tmpfile) or die "Could not open file";
    print $cfg $contents;
    if ($_->{drbg}) {
        print $cfg "random = $_->{drbg}\n";
    }
    if ($_->{cipher}) {
        print $cfg "cipher = $_->{cipher}\n";
    }
    if ($_->{digest}) {
        print $cfg "digest = $_->{digest}\n"
    }
    close $cfg;

    $ENV{OPENSSL_CONF} = $tmpfile;

    ok(comparelines($_->{expected}), $_->{desc});
}

# Check that the stdout output contains the expected values.
sub comparelines {
    my @lines = run(app(["openssl", "list", "--random-instances"]),
                    capture => 1);

    foreach (@_) {
        if ( !grep( /$_/, @lines ) ) {
            print "Cannot find: $_\n";
            return 0;
        }
    }
    return 1;
}
