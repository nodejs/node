#! /usr/bin/env perl
# Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
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

plan tests => ($no_fips ? 0 : 1) + 2;     # FIPS install test + test

my $conf = srctop_file("test", "default.cnf");
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
}

