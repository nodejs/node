#! /usr/bin/env perl
# Copyright 2016-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_file/;
use File::Temp qw(tempfile);

setup("test_sslapi");

plan skip_all => "No TLS/SSL protocols are supported by this OpenSSL build"
    if alldisabled(grep { $_ ne "ssl3" } available_protocols("tls"));

plan tests => 1;

(undef, my $tmpfilename) = tempfile();

ok(run(test(["sslapitest", srctop_file("apps", "server.pem"),
             srctop_file("apps", "server.pem"),
             srctop_file("test", "recipes", "90-test_sslapi_data",
                         "passwd.txt"), $tmpfilename])),
             "running sslapitest");

unlink $tmpfilename;
