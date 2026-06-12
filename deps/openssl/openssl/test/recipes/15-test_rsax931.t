#! /usr/bin/env perl
# Copyright 2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw(:DEFAULT srctop_file);
use OpenSSL::Test::Utils;

BEGIN {
    setup("test_rsax931");
}

plan tests => 6;

my $infile = srctop_file("test", "certs", "sm2.key");
my $inkey = srctop_file("test", "testrsa2048.pem");

ok(run(app(['openssl', 'pkeyutl', '-in', $infile, '-rawin', '-inkey', $inkey,
            '-digest', 'SHA256',
            '-pkeyopt', 'pad-mode:x931',
            '-sign',
            '-out', 'sigx931.txt'])),
  "RSA Sign with x931 padding using SHA256");

ok(run(app(['openssl', 'pkeyutl', '-in', $infile, '-rawin', '-inkey', $inkey,
            '-digest', 'SHA256',
            '-pkeyopt', 'pad-mode:x931',
            '-verify',
            '-sigfile', 'sigx931.txt'])),
  "RSA Verify with x931 padding using SHA256");

ok(!run(app(['openssl', 'pkeyutl', '-in', $infile, '-rawin', '-inkey', $inkey,
             '-digest', 'SHA512',
             '-pkeyopt', 'pad-mode:x931',
             '-verify',
             '-sigfile', 'sigx931.txt'])),
  "RSA Verify with x931 padding fails if digest is different");

ok(!run(app(['openssl', 'pkeyutl', '-in', $infile, '-rawin', '-inkey', $inkey,
             '-digest', 'SHA512-256',
             '-pkeyopt', 'pad-mode:x931',
             '-sign'])),
  "RSA Sign with x931 padding using unsupported digest should fail");

ok(run(app(['openssl', 'pkeyutl', '-in', $infile, '-rawin', '-inkey', $inkey,
            '-digest', 'SHA256',
            '-pkeyopt', 'pad-mode:oaep',
            '-sign',
            '-out', 'sigoaep.txt'])),
  "RSA Sign with oaep padding using SHA256");

ok(!run(app(['openssl', 'pkeyutl', '-in', $infile, '-rawin', '-inkey', $inkey,
             '-digest', 'SHA256',
             '-pkeyopt', 'pad-mode:x931',
             '-verify',
             '-sigfile', 'sigoaep.txt'])),
  "RSA Verify with x931 padding using data signed with oaep padding should fail");
