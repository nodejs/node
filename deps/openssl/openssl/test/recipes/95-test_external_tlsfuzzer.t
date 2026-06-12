#! /usr/bin/env perl
# Copyright 2015-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test;
use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT data_file data_dir bldtop_dir srctop_dir cmdstr/;
use Cwd qw(abs_path);

setup("test_external_tlsfuzzer");

plan skip_all => "No external tests in this configuration"
    if disabled("external-tests");
plan skip_all => "TLSFuzzer tests not available on Windows or VMS"
    if $^O =~ /^(VMS|MSWin32)$/;
plan skip_all => "TLSFuzzer is not properly checked out"
    unless (-d srctop_dir("tlsfuzzer") && -d srctop_dir("tlsfuzzer", "tests"));

$ENV{TESTDATADIR} = abs_path(data_dir());
plan tests => 1;

ok(run(cmd(["sh", data_file("tls-fuzzer-cert.sh")])),
   "running TLSFuzzer tests");
