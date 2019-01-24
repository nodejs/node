#! /usr/bin/perl
#
# Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test::Simple;
use OpenSSL::Test;
use OpenSSL::Test::Utils qw(alldisabled available_protocols);

setup("test_cipherbytes");

my $no_anytls = alldisabled(available_protocols("tls"));

# If we have no protocols, then we also have no supported ciphers.
plan skip_all => "No SSL/TLS protocol is supported by this OpenSSL build."
    if $no_anytls;

simple_test("test_cipherbytes", "cipherbytes_test", "bytes_to_cipherlist");
