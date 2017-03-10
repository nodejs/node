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

setup("test_ec");

plan tests => 5;

require_ok(srctop_file('test','recipes','tconversion.pl'));

ok(run(test(["ectest"])), "running ectest");

 SKIP: {
     skip "Skipping ec conversion test", 3
	 if disabled("ec");

     subtest 'ec conversions -- private key' => sub {
	 tconversion("ec", srctop_file("test","testec-p256.pem"));
     };
     subtest 'ec conversions -- private key PKCS#8' => sub {
	 tconversion("ec", srctop_file("test","testec-p256.pem"), "pkey");
     };
     subtest 'ec conversions -- public key' => sub {
	 tconversion("ec", srctop_file("test","testecpub-p256.pem"), "ec", "-pubin", "-pubout");
     };
}
