#! /usr/bin/env perl
# Copyright 2016-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_file srctop_dir bldtop_dir bldtop_file/;
use File::Temp qw(tempfile);

BEGIN {
setup("test_sslapi");
}

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);

plan skip_all => "No TLS/SSL protocols are supported by this OpenSSL build"
    if alldisabled(grep { $_ ne "ssl3" } available_protocols("tls"));

plan tests =>
    ($no_fips ? 0 : 1)          # sslapitest with fips
    + 1;                        # sslapitest with default provider

(undef, my $tmpfilename) = tempfile();

ok(run(test(["sslapitest", srctop_dir("test", "certs"),
             srctop_file("test", "recipes", "90-test_sslapi_data",
                         "passwd.txt"), $tmpfilename, "default",
             srctop_file("test", "default.cnf")])),
             "running sslapitest");

unless ($no_fips) {
    ok(run(test(["sslapitest", srctop_dir("test", "certs"),
                 srctop_file("test", "recipes", "90-test_sslapi_data",
                             "passwd.txt"), $tmpfilename, "fips",
                 srctop_file("test", "fips-and-base.cnf")])),
                 "running sslapitest");
}

unlink $tmpfilename;
