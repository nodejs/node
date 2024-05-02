#! /usr/bin/env perl
# Copyright 2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw(:DEFAULT srctop_file);

setup('test_bio_readbuffer');

my $pemfile = srctop_file("test", "certs", "leaf.pem");
my $derfile = 'readbuffer_leaf.der';

plan tests => 3;

ok(run(app([ 'openssl', 'x509', '-inform', 'PEM', '-in', $pemfile,
             '-outform', 'DER', '-out', $derfile])),
   "Generate a DER certificate");

ok(run(test(["bio_readbuffer_test", $derfile])),
   "Running bio_readbuffer_test $derfile");

ok(run(test(["bio_readbuffer_test", $pemfile])),
   "Running bio_readbuffer_test $pemfile");
