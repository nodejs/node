#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

setup("test_rsa");

plan tests => 6;

require_ok(srctop_file('test','recipes','tconversion.pl'));

ok(run(test(["rsa_test"])), "running rsatest");

ok(run(app([ 'openssl', 'rsa', '-check', '-in', srctop_file('test', 'testrsa.pem'), '-noout'])), "rsa -check");

 SKIP: {
     skip "Skipping rsa conversion test", 3
	 if disabled("rsa");

     subtest 'rsa conversions -- private key' => sub {
	 tconversion("rsa", srctop_file("test","testrsa.pem"));
     };
     subtest 'rsa conversions -- private key PKCS#8' => sub {
	 tconversion("rsa", srctop_file("test","testrsa.pem"), "pkey");
     };
}

 SKIP: {
     skip "Skipping msblob conversion test", 1
	 if disabled("rsa") || disabled("dsa");

     subtest 'rsa conversions -- public key' => sub {
	 tconversion("msb", srctop_file("test","testrsapub.pem"), "rsa",
		     "-pubin", "-pubout");
     };
}
