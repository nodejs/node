#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;
use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

setup("test_dane");

plan skip_all => "test_dane uses ec which is not supported by this OpenSSL build"
    if disabled("ec");

plan tests => 2;                # The number of tests being performed

ok(run(test(["danetest", "example.com",
             srctop_file("test", "danetest.pem"),
             srctop_file("test", "danetest.in")])), "dane tests");

ok(run(test(["danetest", "server.example",
             srctop_file("test", "certs", "cross-root.pem"),
             srctop_file("test", "dane-cross.in")])), "dane cross CA test");
