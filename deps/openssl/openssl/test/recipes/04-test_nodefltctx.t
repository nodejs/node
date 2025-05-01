#! /usr/bin/env perl
# Copyright 2023The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test::Simple;
use OpenSSL::Test qw/:DEFAULT srctop_file/;
use Cwd qw(abs_path);

setup("test_nodefltctx");

# Load the null provider by default into the default libctx
$ENV{OPENSSL_CONF} = abs_path(srctop_file("test", "null.cnf"));

simple_test("test_nodefltctx", "nodefltctxtest");
