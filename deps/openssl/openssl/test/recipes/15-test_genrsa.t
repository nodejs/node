#! /usr/bin/env perl
# Copyright 2017-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

setup("test_genrsa");

plan tests => 5;

# We want to know that an absurdly small number of bits isn't support
is(run(app([ 'openssl', 'genrsa', '-3', '-out', 'genrsatest.pem', '8'])), 0, "genrsa -3 8");

# Depending on the shared library, we might have different lower limits.
# Let's find it!  This is a simple binary search
# ------------------------------------------------------------
# NOTE: $good may need an update in the future
# ------------------------------------------------------------
note "Looking for lowest amount of bits";
my $bad = 3;                    # Log2 of number of bits (2 << 3  == 8)
my $good = 11;                  # Log2 of number of bits (2 << 11 == 2048)
while ($good > $bad + 1) {
    my $checked = int(($good + $bad + 1) / 2);
    if (run(app([ 'openssl', 'genrsa', '-3', '-out', 'genrsatest.pem',
                  2 ** $checked ], stderr => undef))) {
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

ok(run(app([ 'openssl', 'genrsa', '-3', '-out', 'genrsatest.pem', $good ])),
   "genrsa -3 $good");
ok(run(app([ 'openssl', 'rsa', '-check', '-in', 'genrsatest.pem', '-noout' ])),
   "rsa -check");
ok(run(app([ 'openssl', 'genrsa', '-f4', '-out', 'genrsatest.pem', $good ])),
   "genrsa -f4 $good");
ok(run(app([ 'openssl', 'rsa', '-check', '-in', 'genrsatest.pem', '-noout' ])),
   "rsa -check");
