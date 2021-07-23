#! /usr/bin/env perl
# Copyright 2015-2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

setup("test_rsa");

plan tests => 10;

require_ok(srctop_file('test', 'recipes', 'tconversion.pl'));

ok(run(test(["rsa_test"])), "running rsatest");

run_rsa_tests("pkey");

run_rsa_tests("rsa");

sub run_rsa_tests {
    my $cmd = shift;

    ok(run(app([ 'openssl', $cmd, '-check', '-in', srctop_file('test', 'testrsa.pem'), '-noout'])),
           "$cmd -check" );

     SKIP: {
         skip "Skipping $cmd conversion test", 3
	     if disabled("rsa");

         subtest "$cmd conversions -- private key" => sub {
	     tconversion( -type => $cmd, -prefix => "$cmd-priv",
                          -in => srctop_file("test", "testrsa.pem") );
         };
         subtest "$cmd conversions -- private key PKCS#8" => sub {
	     tconversion( -type => $cmd, -prefix => "$cmd-pkcs8",
                          -in => srctop_file("test", "testrsa.pem"),
                          -args => ["pkey"] );
         };
    }

     SKIP: {
         skip "Skipping msblob conversion test", 1
	     if disabled($cmd) || $cmd eq 'pkey';

         subtest "$cmd conversions -- public key" => sub {
	     tconversion( -type => 'msb', -prefix => "$cmd-msb-pub",
                          -in => srctop_file("test", "testrsapub.pem"),
                          -args => ["rsa", "-pubin", "-pubout"] );
         };
    }
}
