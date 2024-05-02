#! /usr/bin/env perl
# Copyright 2017-2022 The OpenSSL Project Authors. All Rights Reserved.
# Copyright (c) 2017, Oracle and/or its affiliates.  All rights reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test;
use OpenSSL::Test::Utils;

setup("test_locale");

plan skip_all => "Locale tests not available on Windows or VMS"
    if $^O =~ /^(VMS|MSWin32)$/;

plan tests => 3;

ok(run(test(["evp_pkey_ctx_new_from_name"])), "running evp_pkey_ctx_new_from_name without explicit context init");

$ENV{LANG} = "C";
ok(run(test(["localetest"])), "running localetest");

$ENV{LANG} = "tr_TR.UTF-8";
ok(run(test(["localetest"])), "running localetest with Turkish locale");
