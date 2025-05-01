#! /usr/bin/env perl
# Copyright 2020-2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test;

setup("test_app");

plan tests => 7;

ok(run(app(["openssl"])),
   "Run openssl app with no args");

ok(run(app(["openssl", "help"])),
   "Run openssl app with help");

ok(!run(app(["openssl", "-wrong"])),
   "Run openssl app with incorrect arg");

ok(run(app(["openssl", "-help"])),
   "Run openssl app with -help");

ok(run(app(["openssl", "--help"])),
   "Run openssl app with --help");

ok(run(app(["openssl", "-version"])),
   "Run openssl app with -version");

ok(run(app(["openssl", "--version"])),
   "Run openssl app with --version");
