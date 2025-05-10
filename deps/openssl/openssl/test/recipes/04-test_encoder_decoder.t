#! /usr/bin/env perl
# Copyright 2020-2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw/:DEFAULT srctop_dir srctop_file bldtop_dir bldtop_file/;
use OpenSSL::Test::Utils;

BEGIN {
    setup("test_encoder_decoder");
}

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');
use platform;

my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);

my $rsa_key = srctop_file("test", "certs", "ee-key.pem");
my $pss_key = srctop_file("test", "certs", "ca-pss-key.pem");

plan tests => ($no_fips ? 0 : 5) + 2;     # FIPS install test + test

my $conf = srctop_file("test", "default.cnf");

# Check if the specified pattern occurs in the given file
# Returns 1 if the pattern is found and 0 if not
sub find_line_file {
    my ($key, $file) = @_;

    open(my $in, $file) or return -1;
    while (my $line = <$in>) {
        if ($line =~ /$key/) {
            close($in);
            return 1;
        }
    }
    close($in);
    return 0;
}

ok(run(test(["endecode_test", "-rsa", $rsa_key,
                              "-pss", $pss_key,
                              "-config", $conf,
                              "-provider", "default"])));

# Run with non-default library context
ok(run(test(["endecode_test", "-rsa", $rsa_key,
                              "-pss", $pss_key,
                              "-context",
                              "-config", $conf,
                              "-provider", "default"])));

unless ($no_fips) {
    # Run with fips library context
    my $conf = srctop_file("test", "fips-and-base.cnf");
    ok(run(test(["endecode_test", "-rsa", $rsa_key,
                                  "-pss", $pss_key,
                                  "-config", $conf,
                                  "-provider", "fips"])));
SKIP: {
    skip "EC disabled", 2 if disabled("ec");
    ok(run(app([ 'openssl', 'genpkey', '-algorithm', 'EC',
                 '-pkeyopt', 'group:P-256', '-text',
                 '-config', $conf, '-provider', 'fips', '-out', 'ec.txt' ])),
       'Print a FIPS provider EC private key');
    ok(find_line_file('NIST CURVE: P-256', 'ec.txt') == 1,
       'Printing an FIPS provider EC private key');
}
    my $no_des = disabled("des");
SKIP: {
    skip "MD5 disabled", 2 if disabled("md5");
    ok(run(app([ 'openssl', 'genrsa', '-aes128', '-out', 'epki.pem',
                 '-traditional', '-passout', 'pass:pass' ])),
       "rsa encrypted using a non fips algorithm MD5 in pbe");

    my $conf2 = srctop_file("test", "default-and-fips.cnf");
    ok(run(test(['decoder_propq_test', '-config', $conf2,
                 '-provider', 'fips', 'epki.pem'])));
}
}
