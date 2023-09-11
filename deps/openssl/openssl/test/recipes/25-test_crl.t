#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_crl");

plan tests => 10;

require_ok(srctop_file('test','recipes','tconversion.pl'));

my $pem = srctop_file("test/certs", "cyrillic_crl.pem");
my $out = "cyrillic_crl.out";
my $utf = srctop_file("test/certs", "cyrillic_crl.utf8");

subtest 'crl conversions' => sub {
    tconversion( -type => "crl", -in => srctop_file("test","testcrl.pem") );
};

ok(run(test(['crltest'])));

ok(compare1stline([qw{openssl crl -noout -fingerprint -in},
                   srctop_file('test', 'testcrl.pem')],
                  'SHA1 Fingerprint=BA:F4:1B:AD:7A:9B:2F:09:16:BC:60:A7:0E:CE:79:2E:36:00:E7:B2'));
ok(compare1stline([qw{openssl crl -noout -fingerprint -sha256 -in},
                   srctop_file('test', 'testcrl.pem')],
                  'SHA2-256 Fingerprint=B3:A9:FD:A7:2E:8C:3D:DF:D0:F1:C3:1A:96:60:B5:FD:B0:99:7C:7F:0E:E4:34:F5:DB:87:62:36:BC:F1:BC:1B'));
ok(compare1stline([qw{openssl crl -noout -hash -in},
                   srctop_file('test', 'testcrl.pem')],
                  '106cd822'));

ok(compare1stline_stdin([qw{openssl crl -hash -noout}],
                        srctop_file("test","testcrl.pem"),
                        '106cd822'),
   "crl piped input test");

ok(!run(app(["openssl", "crl", "-text", "-in", $pem, "-inform", "DER",
             "-out", $out, "-nameopt", "utf8"])));
ok(run(app(["openssl", "crl", "-text", "-in", $pem, "-inform", "PEM",
            "-out", $out, "-nameopt", "utf8"])));
is(cmp_text($out, srctop_file("test/certs", "cyrillic_crl.utf8")),
   0, 'Comparing utf8 output');

sub compare1stline {
    my ($cmdarray, $str) = @_;
    my @lines = run(app($cmdarray), capture => 1);

    return 1 if $lines[0] =~ m|^\Q${str}\E\R$|;
    note "Got      ", $lines[0];
    note "Expected ", $str;
    return 0;
}

sub compare1stline_stdin {
    my ($cmdarray, $infile, $str) = @_;
    my @lines = run(app($cmdarray, stdin => $infile), capture => 1);

    return 1 if $lines[0] =~ m|^\Q${str}\E\R$|;
    note "Got      ", $lines[0];
    note "Expected ", $str;
    return 0;
}
