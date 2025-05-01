#! /usr/bin/env perl
# Copyright 2021 The OpenSSL Project Authors. All Rights Reserved.
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
use OpenSSL::Test qw/:DEFAULT srctop_file bldtop_dir/;
use Cwd qw(abs_path);

setup("test_pbe");

plan skip_all => "PKCS5 PBE only available in legacy provider"
    if disabled("legacy");

plan tests => 1;

$ENV{OPENSSL_CONF} = abs_path(srctop_file("test", "default-and-legacy.cnf"));

ok(run(test((["pbetest"]))), "Running PBE test");

