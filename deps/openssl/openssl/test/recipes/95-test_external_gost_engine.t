#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test;
use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT data_file bldtop_dir srctop_dir cmdstr/;

setup("test_external_gost_engine");

plan skip_all => "No external tests in this configuration"
    if disabled("external-tests");
plan skip_all => "GOST engine tests not available on Windows or VMS"
    if $^O =~ /^(VMS|MSWin32)$/;
plan skip_all => "GOST engine tests only available in a shared build"
    if disabled("shared");
plan skip_all => "GOST engine tests not supported in out of tree builds"
    if bldtop_dir() ne srctop_dir();

plan tests => 1;

ok(run(cmd(["sh", data_file("gost_engine.sh")])),
   "running GOST engine tests");
