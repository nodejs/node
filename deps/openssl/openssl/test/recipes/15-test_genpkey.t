#! /usr/bin/env perl
# Copyright 2022-2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw/:DEFAULT/;
use OpenSSL::Test::Utils;

setup("test_genpkey");

my @algs = ();
push @algs, qw(RSA) unless disabled("rsa");
push @algs, qw(DSA) unless disabled("dsa");
push @algs, qw(DH DHX) unless disabled("dh");
push @algs, qw(EC) unless disabled("ec");
push @algs, qw(X25519 X448) unless disabled("ecx");
push @algs, qw(SM2) unless disabled("sm2");

plan tests => scalar(@algs);

foreach (@algs) {
    my $alg = $_;

    ok(run(app([ 'openssl', 'genpkey', '-algorithm', $alg, '-help'])),
       "show genpkey pkeyopt values for $alg");
}
