#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;
use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

setup("test_x509aux");

plan skip_all => "test_dane uses ec which is not supported by this OpenSSL build"
    if disabled("ec");

plan tests => 1;                # The number of tests being performed

ok(run(test(["x509aux", 
                srctop_file("test", "certs", "roots.pem"),
                srctop_file("test", "certs", "root+anyEKU.pem"),
                srctop_file("test", "certs", "root-anyEKU.pem"),
                srctop_file("test", "certs", "root-cert.pem")]
        )), "x509aux tests");
