#! /usr/bin/env perl
# Copyright 2019 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw/:DEFAULT data_file/;

setup("test_params_conversion");

my @files = ( "native_types.txt" );

plan tests => scalar(@files);

foreach my $f ( @files ) {
    ok(run(test(["params_conversion_test", data_file("$f")])),
       "running params_conversion_test $f");
}
