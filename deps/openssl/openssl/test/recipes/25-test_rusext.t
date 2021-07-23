#! /usr/bin/env perl
# Copyright 2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_rusext");

plan tests => 5;

require_ok(srctop_file('test', 'recipes', 'tconversion.pl'));
my $pem = srctop_file("test/certs", "grfc.pem");
my $out_msb = "grfc.msb";
my $out_utf8 = "grfc.utf8";

ok(run(app(["openssl", "x509", "-text", "-in", $pem, "-out", $out_msb,
            "-nameopt", "esc_msb", "-certopt", "no_pubkey"])));
is(cmp_text($out_msb, srctop_file('test', 'recipes', '25-test_rusext_data', 'grfc.msb')),
   0, 'Comparing esc_msb output');
ok(run(app(["openssl", "x509", "-text", "-in", $pem, "-out", $out_utf8,
            "-nameopt", "utf8", "-certopt", "no_pubkey"])));
is(cmp_text($out_utf8, srctop_file('test', 'recipes', '25-test_rusext_data', 'grfc.utf8')),
   0, 'Comparing utf8 output');
