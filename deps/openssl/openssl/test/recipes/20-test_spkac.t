#! /usr/bin/env perl
# Copyright 2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use File::Spec;
use File::Basename;
use OpenSSL::Test qw/:DEFAULT srctop_file ok_nofips/;
use OpenSSL::Test::Utils;

setup("test_spkac");

plan skip_all => "RSA is not supported by this OpenSSL build"
    if disabled("rsa");

plan tests => 4;

# For the tests below we use the cert itself as the TBS file

SKIP: {
    skip "MD5 is not supported by this OpenSSL build", 2
        if disabled("md5");

    ok(run(app([ 'openssl', 'spkac', '-key', srctop_file("test", "testrsa.pem"),
                 '-out', 'spkac-md5.pem'])),
               "SPKAC MD5");
    ok(run(app([ 'openssl', 'spkac', '-in', 'spkac-md5.pem'])),
               "SPKAC MD5 verify");
}

ok(run(app([ 'openssl', 'spkac', '-key', srctop_file("test", "testrsa.pem"),
             '-out', 'spkac-sha256.pem', '-digest', 'sha256'])),
           "SPKAC SHA256");
ok(run(app([ 'openssl', 'spkac', '-in', 'spkac-sha256.pem'])),
           "SPKAC SHA256 verify");
