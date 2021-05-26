#! /usr/bin/env perl
# Copyright 2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test::Simple;
use OpenSSL::Test qw/:DEFAULT srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;
use Cwd qw(abs_path);

setup("test_encoder_decoder_legacy");

plan skip_all => "Not available in a no-deprecated build"
    if disabled("deprecated");
plan tests => 1;


$ENV{OPENSSL_MODULES} = abs_path(bldtop_dir("providers"));
$ENV{OPENSSL_CONF} = abs_path(srctop_file("test", "default.cnf"));

ok(run(test(["endecoder_legacy_test"])));
