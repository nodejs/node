#! /usr/bin/env perl
# Copyright 2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test;
use OpenSSL::Test::Simple;
use OpenSSL::Test::Utils;

setup("test_evp_pkey_dhkem");

plan skip_all => "This test is unsupported in a no-ec build" if disabled("ec");

simple_test("test_evp_pkey_dhkem", "evp_pkey_dhkem_test");
