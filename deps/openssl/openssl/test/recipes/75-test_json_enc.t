#! /usr/bin/env perl
# Copyright 2023-2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test;
use OpenSSL::Test::Utils;

setup("test_json");

plan skip_all => "JSON encoder is not needed by this OpenSSL build (no QUIC)"
    if disabled('qlog');

plan tests => 1;

ok(run(test(["json_test"])));
