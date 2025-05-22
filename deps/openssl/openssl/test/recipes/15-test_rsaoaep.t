#! /usr/bin/env perl
# Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw(:DEFAULT data_file bldtop_dir srctop_file srctop_dir bldtop_file);
use OpenSSL::Test::Utils;
use File::Compare qw/compare_text/;

BEGIN {
    setup("test_rsaoaep");
}
use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

my $no_check = disabled('fips-securitychecks');

plan tests =>
    ($no_check ? 0 : 1)         # FIPS security check
    + 9;

my @prov = ( );
my $provconf = srctop_file("test", "fips-and-base.cnf");
my $provpath = bldtop_dir("providers");
my $msg_file = data_file("plain_text");
my $enc1_file = "enc1.bin";
my $enc2_file = "enc2.bin";
my $enc3_file = "enc3.bin";
my $dec1_file = "dec1.txt";
my $dec2_file = "dec2.txt";
my $dec3_file = "dec3.txt";
my $key_file = srctop_file("test", "testrsa2048.pem");
my $small_key_file = srctop_file("test", "testrsa.pem");

$ENV{OPENSSL_TEST_LIBCTX} = "1";

unless ($no_check) {
    @prov = ( "-provider-path", $provpath, "-config", $provconf );
    ok(!run(app(['openssl', 'pkeyutl',
                 @prov,
                 '-encrypt',
                 '-in', $msg_file,
                 '-inkey', $small_key_file,
                 '-pkeyopt', 'pad-mode:oaep',
                 '-pkeyopt', 'oaep-label:123',
                 '-pkeyopt', 'digest:sha1',
                 '-pkeyopt', 'mgf1-digest:sha1',
                 '-out', $enc1_file])),
       "RSA OAEP Encryption with a key smaller than 2048 in fips mode should fail");
}

ok(run(app(['openssl', 'pkeyutl',
            @prov,
            '-encrypt',
            '-in', $msg_file,
            '-inkey', $key_file,
            '-pkeyopt', 'pad-mode:oaep',
            '-pkeyopt', 'oaep-label:123',
            '-pkeyopt', 'digest:sha1',
            '-pkeyopt', 'mgf1-digest:sha1',
            '-out', $enc1_file])),
   "RSA OAEP Encryption");

ok(!run(app(['openssl', 'pkeyutl',
             @prov,
             '-encrypt',
             '-in', $key_file,
             '-inkey', $key_file,
             '-pkeyopt', 'pad-mode:oaep',
             '-pkeyopt', 'oaep-label:123',
             '-pkeyopt', 'digest:sha256',
             '-pkeyopt', 'mgf1-digest:sha1'])),
   "RSA OAEP Encryption should fail if the message is larger than the rsa modulus");

ok(run(app(['openssl', 'pkeyutl',
            @prov,
            '-decrypt',
            '-inkey', $key_file,
            '-pkeyopt', 'pad-mode:oaep',
            '-pkeyopt', 'oaep-label:123',
            '-pkeyopt', 'digest:sha1',
            '-pkeyopt', 'mgf1-digest:sha1',
            '-in', $enc1_file,
            '-out', $dec1_file]))
    && compare_text($dec1_file, $msg_file) == 0,
    "RSA OAEP Decryption");

ok(!run(app(['openssl', 'pkeyutl',
             @prov,
             '-decrypt',
             '-inkey', $key_file,
             '-pkeyopt', 'pad-mode:oaep',
             '-pkeyopt', 'oaep-label:123',
             '-pkeyopt', 'digest:sha256',
             '-pkeyopt', 'mgf1-digest:sha224',
             '-in', $enc1_file])),
    "Incorrect digest for RSA OAEP Decryption");

ok(!run(app(['openssl', 'pkeyutl',
             @prov,
             '-decrypt',
             '-inkey', $key_file,
             '-pkeyopt', 'pad-mode:oaep',
             '-pkeyopt', 'oaep-label:123',
             '-pkeyopt', 'digest:sha1',
             '-pkeyopt', 'mgf1-digest:sha224',
             '-in', $enc1_file])),
    "Incorrect mgf1-digest for RSA OAEP Decryption");

ok(run(app(['openssl', 'pkeyutl',
            @prov,
            '-encrypt',
            '-in', $msg_file,
            '-inkey', $key_file,
            '-pkeyopt', 'pad-mode:oaep',
            '-pkeyopt', 'oaep-label:123',
            '-pkeyopt', 'digest:sha1',
            '-pkeyopt', 'mgf1-digest:sha1',
            '-out', $enc2_file]))
    && compare_text($enc2_file, $enc1_file) != 0,
   "RSA OAEP Encryption should generate different encrypted data");

ok(run(app(['openssl', 'pkeyutl',
            @prov,
            '-decrypt',
            '-inkey', $key_file,
            '-pkeyopt', 'pad-mode:oaep',
            '-pkeyopt', 'oaep-label:123',
            '-in', $enc2_file,
            '-out', $dec2_file]))
    && compare_text($dec2_file, $msg_file) == 0,
    "RSA OAEP Decryption with default digests");

ok(run(app(['openssl', 'pkeyutl',
            @prov,
            '-encrypt',
            '-in', $msg_file,
            '-inkey', $key_file,
            '-pkeyopt', 'pad-mode:oaep',
            '-pkeyopt', 'oaep-label:123',
            '-out', $enc3_file])),
   "RSA OAEP Encryption with default digests");

ok(run(app(['openssl', 'pkeyutl',
            @prov,
            '-decrypt',
            '-inkey', $key_file,
            '-pkeyopt', 'pad-mode:oaep',
            '-pkeyopt', 'oaep-label:123',
            '-pkeyopt', 'digest:sha1',
            '-pkeyopt', 'mgf1-digest:sha1',
            '-in', $enc3_file,
            '-out', $dec3_file]))
    && compare_text($dec3_file, $msg_file) == 0,
    "RSA OAEP Decryption with explicit default digests");
