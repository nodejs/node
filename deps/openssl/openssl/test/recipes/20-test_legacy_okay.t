#! /usr/bin/env perl
# Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test;

setup("test_legacy");

plan tests => 3;

ok(run(app(['openssl', 'rand', '-out', 'rand.txt', '256'])), "Generate random file");

ok(run(app(['openssl', 'dgst', '-sha256', 'rand.txt'])), "Generate a digest");

ok(!run(app(['openssl', 'dgst', '-sha256', '-propquery', 'foo=1',
             'rand.txt'])), "Fail to generate a digest");
