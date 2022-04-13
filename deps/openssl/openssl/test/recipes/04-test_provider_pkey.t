#! /usr/bin/env perl
# Copyright 2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use File::Spec;
use OpenSSL::Test::Simple;

# We must ensure that OPENSSL_CONF points at an empty file.  Otherwise, we
# risk that the configuration file contains statements that load providers,
# which defeats the purpose of this test.  The NUL device is good enough.
$ENV{OPENSSL_CONF} = File::Spec->devnull();

simple_test("test_provider_pkey", "provider_pkey_test");
