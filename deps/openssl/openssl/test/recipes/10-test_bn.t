#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use Math::BigInt;

use OpenSSL::Test qw/:DEFAULT data_file/;

setup("test_bn");

my @files = (
    "bnexp.txt", "bnmod.txt", "bnmul.txt", "bnshift.txt", "bnsum.txt"
    );
plan tests => 1 + scalar(@files);

foreach my $f ( @files ) {
    ok(run(test(["bntest", data_file($f)])),
        "running bntest $f");
}
ok(run(test(["bntest"])), "running bntest");
