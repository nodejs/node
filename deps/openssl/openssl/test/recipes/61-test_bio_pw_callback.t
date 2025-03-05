#! /usr/bin/env perl
# Copyright 2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw(:DEFAULT data_file);

setup('test_bio_pw_callback');

plan tests => 1;

my $private_key_path = data_file("private_key.pem");
ok(run(test(["bio_pw_callback_test", "-keyfile", $private_key_path])),
   "Running bio_pw_callback_test");
