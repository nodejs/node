#! /usr/bin/env perl
# Copyright 2022-2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test qw/:DEFAULT srctop_file result_dir data_file/;
use OpenSSL::Test::Utils;
use File::Temp qw(tempfile);
use File::Path 2.00 qw(rmtree mkpath);

setup("test_quic_multistream");

plan skip_all => "QUIC protocol is not supported by this OpenSSL build"
    if disabled('quic');

plan tests => 2;

my $qlog_output;
if (!disabled('qlog')) {
    $qlog_output = result_dir("qlog-output");
    print "# Writing qlog output to $qlog_output\n";
    rmtree($qlog_output, { safe => 1 });
    mkdir($qlog_output);
    $ENV{QLOGDIR} = $qlog_output;
}

$ENV{OSSL_QFILTER} = "* -quic:unknown_event quic:another_unknown_event";

ok(run(test(["quic_multistream_test",
             srctop_file("test", "certs", "servercert.pem"),
             srctop_file("test", "certs", "serverkey.pem")])));

SKIP: {
    skip "no qlog", 1 if disabled('qlog');
    skip "not running CI tests", 1 unless $ENV{OSSL_RUN_CI_TESTS};

    subtest "check qlog output" => sub {
        plan tests => 1;

        ok(run(cmd([data_file("verify-qlog.py")], exe_shell => "python3")),
               "running qlog verification script");
    };
}
