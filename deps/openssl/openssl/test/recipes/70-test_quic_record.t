#! /usr/bin/env perl
# Copyright 2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test;
use OpenSSL::Test::Utils;

setup("test_quic_record");

plan skip_all => "QUIC protocol is not supported by this OpenSSL build"
    if disabled('quic');

plan skip_all => "These tests are not supported in a fuzz build"
    if config('options') =~ /-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION|enable-fuzz-afl/;

plan tests => 1;

ok(run(test(["quic_record_test"])));
