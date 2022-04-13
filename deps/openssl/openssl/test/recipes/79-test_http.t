#! /usr/bin/env perl
# Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

setup("test_http");

plan tests => 2;

SKIP: {
    skip "sockets disabled", 1 if disabled("sock");
    skip "OCSP disabled", 1 if disabled("ocsp");
    my $cmd = [qw{openssl ocsp -index any -port 0}];
    my @output = run(app($cmd), capture => 1);
    $output[0] =~ s/\r\n/\n/g;
    ok($output[0] =~ /^ACCEPT (0.0.0.0|\[::\]):(\d+?) PID=(\d+)$/
       && $2 >= 1024 && $3 > 0,
       "HTTP server auto-selects and reports local port >= 1024 and pid > 0");
}

ok(run(test(["http_test", srctop_file("test", "certs", "ca-cert.pem")])));
