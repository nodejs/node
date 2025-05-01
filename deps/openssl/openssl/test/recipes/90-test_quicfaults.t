#! /usr/bin/env perl
# Copyright 2022-2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_dir bldtop_dir/;

BEGIN {
setup("test_quicfaults");
}

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

plan skip_all => "QUIC protocol is not supported by this OpenSSL build"
    if disabled('quic');

plan skip_all => "These tests are not supported in a fuzz build"
    if config('options') =~ /-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION|enable-fuzz-afl/;

plan tests => 2;

ok(run(test(["quicfaultstest", srctop_dir("test", "certs")])),
   "running quicfaultstest");

ok(run(test(["quic_newcid_test", srctop_dir("test", "certs")])),
   "running quic_newcid_test");
