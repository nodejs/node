#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test;
use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT bldtop_file srctop_file cmdstr/;

setup("test_external_boringssl");

plan skip_all => "No external tests in this configuration"
    if disabled("external-tests");
plan skip_all => "BoringSSL runner not detected"
    if !$ENV{BORING_RUNNER_DIR};

plan tests => 1;

indir $ENV{BORING_RUNNER_DIR} => sub {
    ok(run(cmd(["go", "test", "-shim-path",
                bldtop_file("test", "ossl_shim", "ossl_shim"),
                "-shim-config",
                srctop_file("test", "ossl_shim", "ossl_config.json"),
                "-pipe", "-allow-unimplemented"]), prefix => "go test: "),
       "running BoringSSL tests");
}, create => 0, cleanup => 0;
