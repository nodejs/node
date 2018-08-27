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

setup("test_pkcs7");

plan tests => 3;

require_ok(srctop_file('test','recipes','tconversion.pl'));

subtest 'pkcs7 conversions -- pkcs7' => sub {
    tconversion("p7", srctop_file("test", "testp7.pem"), "pkcs7");
};
subtest 'pkcs7 conversions -- pkcs7d' => sub {
    tconversion("p7d", srctop_file("test", "pkcs7-1.pem"), "pkcs7");
};
