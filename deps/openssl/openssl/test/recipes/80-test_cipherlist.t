#! /usr/bin/env perl
#
# Copyright 2016-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test::Simple;
use OpenSSL::Test qw(:DEFAULT openssl_versions);
use OpenSSL::Test::Utils qw(alldisabled available_protocols);

setup("test_cipherlist");

my ($build_version, $library_version) = openssl_versions();
plan skip_all =>
    "This test recipe isn't supported when doing regression testing"
    if $build_version ne $library_version;

my $no_anytls = alldisabled(available_protocols("tls"));

# If we have no protocols, then we also have no supported ciphers.
plan skip_all => "No SSL/TLS protocol is supported by this OpenSSL build."
    if $no_anytls;

simple_test("test_cipherlist", "cipherlist_test", "cipherlist");
