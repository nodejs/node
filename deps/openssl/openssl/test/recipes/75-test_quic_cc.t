#! /usr/bin/env perl
# Copyright 2022-2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test;
use OpenSSL::Test::Utils;

setup("test_quic_cc");

plan skip_all => "QUIC protocol is not supported by this OpenSSL build"
    if disabled('quic');

plan tests => 1;

ok(run(test(["quic_cc_test"])));
