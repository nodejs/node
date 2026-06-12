#! /usr/bin/env perl
# Copyright 2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test;
use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT data_file bldtop_dir srctop_dir srctop_file/;

setup("test_external_cf_quiche");

plan skip_all => "No external tests in this configuration"
    if disabled("external-tests");
plan skip_all => "Cloudflare quiche tests not available on Windows or VMS"
    if $^O =~ /^(VMS|MSWin32)$/;
plan skip_all => "Cloudflare quiche tests only available with QUIC support"
    if disabled("quic");
plan skip_all => "Cloudflare & Boringssl not checked out"
    if ! -f srctop_file("cloudflare-quiche", "quiche", "deps", "boringssl", "LICENSE");

plan tests => 3;

ok(run(cmd(["sh", data_file("quiche-build.sh")])),
   "running Cloudflare quiche build");

ok(run(cmd(["sh", data_file("quiche-server.sh")])),
   "running Cloudflare quiche server");

ok(run(test(["quic_client_test"])),
   "running quic_client_test");

open my $fh, '<', "server.pid"
    or die "Error opening server.pid - $!\n";
$serverpid = <$fh>;
close($fh);

kill('TERM', $serverpid);
sleep(1);
kill('KILL', $serverpid);
waitpid($serverpid, 0);
