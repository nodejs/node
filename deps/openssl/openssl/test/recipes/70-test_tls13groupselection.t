#! /usr/bin/env perl
# Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test::Simple;
use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils qw(disabled);

setup("test_tls13groupselection");

plan skip_all => "needs TLSv1.3 enabled"
    if disabled("tls1_3");

plan skip_all => "needs ECX enabled"
    if disabled("ecx");


plan tests => 1;

ok(run(test(["tls13groupselection_test", srctop_file("apps", "server.pem"),
             srctop_file("apps", "server.pem")])),
   "running tls13groupselection_test");
