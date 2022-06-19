#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;
use OpenSSL::Test;
use OpenSSL::Test::Utils;

plan tests => 3;
setup("test_rand");

ok(run(test(["rand_test"])));
ok(run(test(["drbgtest"])));
ok(run(test(["rand_status_test"])));
