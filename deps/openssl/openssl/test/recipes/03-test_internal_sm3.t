#! /usr/bin/env perl
# Copyright 2021-2022 The OpenSSL Project Authors. All Rights Reserved.
# Copyright 2021 [UnionTech](https://www.uniontech.com). All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test;              # get 'plan'
use OpenSSL::Test::Simple;

setup("test_internal_sm3");

simple_test("test_internal_sm3", "sm3_internal_test", "sm3");
