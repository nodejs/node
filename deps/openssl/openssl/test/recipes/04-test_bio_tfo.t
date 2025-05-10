#! /usr/bin/env perl
# Copyright 2016-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test;
use OpenSSL::Test::Simple;
use OpenSSL::Test::Utils;

setup("test_bio_tfo");

plan skip_all => "This test requires enable-tfo" if disabled("tfo");

simple_test("test_bio_tfo", "bio_tfo_test");
