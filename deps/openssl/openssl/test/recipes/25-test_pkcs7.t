#! /usr/bin/env perl
# Copyright 2015-2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT srctop_file data_file/;

setup("test_pkcs7");

plan tests => 7;

require_ok(srctop_file('test','recipes','tconversion.pl'));

my @path = qw(test certs);
my $pemfile = "grfc.pem";
my $p7file = "grfc.p7b";
my $out = "grfc.out";

subtest 'pkcs7 conversions -- pkcs7' => sub {
    tconversion( -type => 'p7', -in => srctop_file("test", "testp7.pem"),
                 -args => ["pkcs7"] );
};
subtest 'pkcs7 conversions -- pkcs7d' => sub {
    tconversion( -type => 'p7d', -in => srctop_file("test", "pkcs7-1.pem"),
                 -args => ["pkcs7"] );
};
ok(run(app(["openssl", "crl2pkcs7", "-nocrl",
            "-certfile", srctop_file(@path, $pemfile),
            "-out", $p7file])));
ok(run(app(["openssl", "pkcs7", "-print_certs", "-quiet",
            "-in", $p7file,
            "-out", $out])));
is(cmp_text($out, data_file('grfc.out')),
    0, 'Comparing output');

my $malformed = data_file('malformed.pkcs7');
ok(run(app(["openssl", "pkcs7", "-in", $malformed])));
