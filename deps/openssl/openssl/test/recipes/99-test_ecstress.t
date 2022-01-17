#! /usr/bin/env perl
# Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test;
use OpenSSL::Test::Utils;

setup("test_ecstress");

plan tests => 1;

SKIP: {
    skip "Skipping EC stress test", 1
        if ! exists $ENV{'ECSTRESS'};
    ok(run(test(["ecstresstest"])), "running ecstresstest");
}
