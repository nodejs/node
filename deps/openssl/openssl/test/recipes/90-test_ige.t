#! /usr/bin/env perl
# Copyright 2015-2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test::Simple;
use OpenSSL::Test;
use OpenSSL::Test::Utils;

setup("test_ige");

plan skip_all => "AES_ige support is disabled in this build"
    if disabled('deprecated-3.0');

simple_test("test_ige", "igetest");
