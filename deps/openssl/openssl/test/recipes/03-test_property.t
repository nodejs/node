#! /usr/bin/env perl
# Copyright 2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test;
use OpenSSL::Test::Utils;

setup('test_property');

plan tests => 2;

ok(run(test(["property_test"])), "running property_test");

ok(run(test(["user_property_test"])), "running user_property_test");
