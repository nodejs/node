#!/usr/bin/env perl
# Copyright 2016-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use if $^O ne "VMS", 'File::Glob' => qw/glob/;
use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

setup("test_fuzz");

my @fuzzers = ('asn1', 'asn1parse', 'bignum', 'bndiv', 'conf', 'crl', 'server', 'x509');
if (!disabled("cms")) {
    push @fuzzers, 'cms';
}
if (!disabled("ct")) {
    push @fuzzers, 'ct';
}
plan tests => scalar @fuzzers;

foreach my $f (@fuzzers) {
    subtest "Fuzzing $f" => sub {
        my @dirs = glob(srctop_file('fuzz', 'corpora', $f));
        push @dirs, glob(srctop_file('fuzz', 'corpora', "$f-*"));

        plan skip_all => "No corpora for $f-test" unless @dirs;

        plan tests => scalar @dirs;

        foreach (@dirs) {
            ok(run(fuzz(["$f-test", $_])));
        }
    }
}
