#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

setup("test_engine");

my @path = qw(test certs);

plan skip_all => "engines are deprecated"
    if disabled('deprecated-3.0');

plan tests => 1;
ok(run(test(["enginetest", srctop_file(@path, "root-cert.pem")])),
   "running enginetest");
