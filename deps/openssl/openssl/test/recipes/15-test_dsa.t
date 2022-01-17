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

setup("test_dsa");

plan skip_all => 'DSA is not supported in this build' if disabled('dsa');
plan tests => 7;

require_ok(srctop_file('test','recipes','tconversion.pl'));

ok(run(test(["dsatest"])), "running dsatest");
ok(run(test(["dsa_no_digest_size_test"])),
   "running dsa_no_digest_size_test");

subtest "dsa conversions using 'openssl dsa' -- private key" => sub {
    tconversion( -type => 'dsa', -prefix => 'dsa-priv',
                 -in => srctop_file("test","testdsa.pem") );
};
subtest "dsa conversions using 'openssl dsa' -- public key" => sub {
    tconversion( -type => 'msb', -prefix => 'dsa-msb-pub',
                 -in => srctop_file("test","testdsapub.pem"),
                 -args => ["dsa", "-pubin", "-pubout"] );
};

subtest "dsa conversions using 'openssl pkey' -- private key PKCS#8" => sub {
    tconversion( -type => 'dsa', -prefix => 'dsa-pkcs8',
                 -in => srctop_file("test","testdsa.pem"),
                 -args => ["pkey"] );
};
subtest "dsa conversions using 'openssl pkey' -- public key" => sub {
    tconversion( -type => 'dsa', -prefix => 'dsa-pkey-pub',
                 -in => srctop_file("test","testdsapub.pem"),
                 -args => ["pkey", "-pubin", "-pubout"] );
};
