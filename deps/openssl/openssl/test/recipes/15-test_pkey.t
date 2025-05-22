#! /usr/bin/env perl
# Copyright 2022-2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test::Utils;
use File::Copy;
use File::Compare qw(compare_text);
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_pkey");

plan tests => 5;

my @app = ('openssl', 'pkey');

my $in_key = srctop_file('test', 'certs', 'root-key.pem');
my $pass = 'pass:password';

subtest "=== pkey typical en-/decryption (using AES256-CBC) ===" => sub {
    plan tests => 4;

    my $encrypted_key = 'encrypted_key.pem';
    my $decrypted_key = 'decrypted_key.pem';

    ok(run(app([@app, '-aes256', '-in', $in_key, '-out', $encrypted_key,
                '-passout', $pass])),
       "encrypt key (using aes-256-cbc)");

    ok(run(app(['openssl', 'asn1parse', '-in', $encrypted_key,
                '-offset', '34', '-length', '18'])), # incl. 2 bytes header
       "encrypted key has default PBKDF2 PARAM 'salt length' 16");

    ok(run(app([@app, '-in', $encrypted_key, '-out', $decrypted_key,
                '-passin', $pass])),
       "decrypt key");
    is(compare_text($in_key, $decrypted_key), 0,
       "Same file contents after encrypting and decrypting in separate files");
};

subtest "=== pkey handling of identical input and output files (using 3DES) and -traditional ===" => sub {
    plan skip_all => "DES not supported in this build" if disabled("des");
    plan tests => 4;

    my $inout = 'inout.pem';

    copy($in_key, $inout);
    ok(run(app([@app, '-des3', '-traditional', '-in', $inout, '-out', $inout,
                '-passout', $pass])),
       "encrypt using identical infile and outfile");

    sub file_line_contains { grep /$_[0]/, ((open F, $_[1]), <F>, close F) }
    ok(file_line_contains("DEK-Info: DES-EDE3-CBC,", $inout),
       "traditional encoding contains \"DEK-Info: DES-EDE3-CBC,\"");

    ok(run(app([@app, '-in', $inout, '-out', $inout, '-passin', $pass])),
       "decrypt using identical infile and outfile");
    is(compare_text($in_key, $inout), 0,
       "Same file contents after encrypting and decrypting using same file");
};

subtest "=== pkey handling of public keys (Ed25519) ===" => sub {
    plan skip_all => "ECX not supported inthis build" if disabled("ecx");
    plan tests => 6;

    my $in_ed_key = srctop_file('test', 'certs', 'root-ed25519.privkey.pem');
    my $in_pubkey = srctop_file('test', 'certs', 'root-ed25519.pubkey.pem');

    my $pub_out1 = 'pub1.pem';
    ok(run(app([@app, '-in', $in_ed_key, '-pubout', '-out', $pub_out1])),
       "extract public key");
    is(compare_text($in_pubkey, $pub_out1), 0,
       "extracted public key is same as original public key");

    my $pub_out2 = 'pub2.pem';
    ok(run(app([@app, '-in', $in_pubkey, '-pubin', '-pubout', '-out', $pub_out2])),
       "read public key from pubfile");
    is(compare_text($in_pubkey, $pub_out2), 0,
       "public key read using pubfile is same as original public key");

    my $pub_out3 = 'pub3.pem';
    ok(run(app([@app, '-in', $in_ed_key, '-pubin', '-pubout', '-out', $pub_out3])),
       "extract public key from pkey file with -pubin");
    is(compare_text($in_pubkey, $pub_out3), 0,
       "public key extraced from pkey file with -pubin is same as original");
};


subtest "=== pkey handling of DER encoding ===" => sub {
    plan tests => 4;

    my $der_out = 'key.der';
    my $pem_out = 'key.pem';
    ok(run(app([@app, '-in', $in_key, '-outform', 'DER',
                 '-out', $der_out])),
       "write DER-encoded pkey");

    ok(run(app(['openssl', 'asn1parse', '-in', $der_out, '-inform', 'DER',
                 '-offset', '268', '-length', '5'])), # incl. 2 bytes header
       "see if length of the modulus encoding is included");

    ok(run(app([@app, '-in', $der_out, '-inform', 'DER',
                 '-out', $pem_out])),
       "read DER-encoded key");
    is(compare_text($in_key, $pem_out), 0,
       "Same file contents after converting to DER and back");
};

subtest "=== pkey text output ===" => sub {
    plan tests => 3;

    ok((grep /BEGIN PRIVATE KEY/,
        run(app([@app, '-in', $in_key, '-text']), capture => 1)),
        "pkey text output contains PEM header");

    ok(!(grep /BEGIN PRIVATE KEY/,
        run(app([@app, '-in', $in_key, '-text', '-noout']), capture => 1)),
        "pkey text output with -noout does not contain PEM header");

    ok((grep /Private-Key:/,
        run(app([@app, '-in', $in_key, '-text', '-noout']), capture => 1)),
        "pkey text output (even with -noout) contains \"Private-Key:\"");
};
