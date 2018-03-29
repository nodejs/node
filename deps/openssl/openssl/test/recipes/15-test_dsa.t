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

setup("test_dsa");

plan tests => 6;

require_ok(srctop_file('test','recipes','tconversion.pl'));

ok(run(test(["dsatest"])), "running dsatest");
ok(run(test(["dsatest", "-app2_1"])), "running dsatest -app2_1");

 SKIP: {
     skip "Skipping dsa conversion test", 3
	 if disabled("dsa");

     subtest 'dsa conversions -- private key' => sub {
	 tconversion("dsa", srctop_file("test","testdsa.pem"));
     };
     subtest 'dsa conversions -- private key PKCS#8' => sub {
	 tconversion("dsa", srctop_file("test","testdsa.pem"), "pkey");
     };
     subtest 'dsa conversions -- public key' => sub {
	 tconversion("msb", srctop_file("test","testdsapub.pem"), "dsa",
		         "-pubin", "-pubout");
     };
}
