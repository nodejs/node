#! /usr/bin/env perl
# Copyright 2017-2023 The OpenSSL Project Authors. All Rights Reserved.
# Copyright 2017 BaishanCloud. All rights reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT data_file/;
use OpenSSL::Test::Utils;

setup("test_mp_rsa");

my @test_param = (
    # 3 primes, 2048-bit
    {
        primes => '3',
        bits => '2048',
    },
    # 4 primes, 4096-bit
    {
        primes => '4',
        bits => '4096',
    },
    # 5 primes, 8192-bit
    {
        primes => '5',
        bits => '8192',
    },
);

plan tests => 2 + scalar(@test_param) * 5 * 2;

ok(run(test(["rsa_mp_test"])), "running rsa multi prime test");

ok(run(app(['openssl', 'pkey', '-noout', '-check', '-in',
            data_file('rsamplcm.pem')])), "checking lcm in key check");

my $cleartext = data_file("plain_text");

# genrsa
run_mp_tests(0);
# evp
run_mp_tests(1);

sub run_mp_tests {
    my $evp = shift;

    foreach my $param (@test_param) {
        my $primes = $param->{primes};
        my $bits = $param->{bits};
        my $name = ($evp ? "evp" : "") . "${bits}p${primes}";

        if ($evp) {
            ok(run(app([ 'openssl', 'genpkey', '-out', "rsamptest-$name.pem",
                         '-algorithm', 'RSA',
                         '-pkeyopt', "rsa_keygen_primes:$primes",
                         '-pkeyopt', "rsa_keygen_bits:$bits"])),
               "genrsa $name");
            ok(run(app([ 'openssl', 'pkey', '-check',
                         '-in', "rsamptest-$name.pem", '-noout'])),
               "rsa -check $name");
            ok(run(app([ 'openssl', 'pkeyutl', '-inkey', "rsamptest-$name.pem",
                         '-encrypt', '-in', $cleartext,
                         '-out', "rsamptest-$name.enc" ])),
               "rsa $name encrypt");
            ok(run(app([ 'openssl', 'pkeyutl', '-inkey', "rsamptest-$name.pem",
                         '-decrypt', '-in', "rsamptest-$name.enc",
                         '-out', "rsamptest-$name.dec" ])),
               "rsa $name decrypt");
        } else {
            ok(run(app([ 'openssl', 'genrsa', '-out', "rsamptest-$name.pem",
                         '-primes', $primes, $bits])), "genrsa $name");
            ok(run(app([ 'openssl', 'rsa', '-check',
                         '-in', "rsamptest-$name.pem", '-noout'])),
               "rsa -check $name");
            if (!disabled('deprecated-3.0')) {
                ok(run(app([ 'openssl', 'rsautl', '-inkey', "rsamptest-$name.pem",
                             '-encrypt', '-in', $cleartext,
                             '-out', "rsamptest-$name.enc" ])),
                   "rsa $name encrypt");
                ok(run(app([ 'openssl', 'rsautl', '-inkey', "rsamptest-$name.pem",
                             '-decrypt', '-in', "rsamptest-$name.enc",
                             '-out', "rsamptest-$name.dec" ])),
                   "rsa $name decrypt");
            } else {
                ok(run(app([ 'openssl', 'pkeyutl', '-inkey', "rsamptest-$name.pem",
                             '-encrypt', '-in', $cleartext,
                             '-out', "rsamptest-$name.enc" ])),
                   "rsa $name encrypt");
                ok(run(app([ 'openssl', 'pkeyutl', '-inkey', "rsamptest-$name.pem",
                             '-decrypt', '-in', "rsamptest-$name.enc",
                             '-out', "rsamptest-$name.dec" ])),
                   "rsa $name decrypt");
            }
        }
        ok(check_msg("rsamptest-$name.dec"), "rsa $name check result");
    }
}

sub check_msg {
    my $decrypted = shift;
    my $msg;
    my $dec;

    open(my $fh, "<", $cleartext) or return 0;
    binmode $fh;
    read($fh, $msg, 10240);
    close $fh;
    open($fh, "<", $decrypted ) or return 0;
    binmode $fh;
    read($fh, $dec, 10240);
    close $fh;

    if ($msg ne $dec) {
        print STDERR "cleartext and decrypted are not the same";
        return 0;
    }
    return 1;
}
