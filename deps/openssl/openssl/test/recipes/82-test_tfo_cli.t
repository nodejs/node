#! /usr/bin/env perl
# Copyright 2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use IPC::Open2;
use OpenSSL::Test qw/:DEFAULT srctop_file bldtop_file/;
use OpenSSL::Test::Utils;

setup("test_tfo");

plan skip_all => "test_tfo_cli needs tfo enabled" if disabled("tfo");
plan skip_all => "test_tfo_cli needs sock enabled" if disabled("sock");
plan skip_all => "test_tfo_cli needs tls < 1.3 enabled"
    if disabled("tls1") && disabled("tls1_1") && disabled("tls1_2");
plan skip_all => "test_tfo_cli does not run on Windows nor VMS"
    if $^O =~ /^(VMS|MSWin32|msys)$/;

plan tests => 8;

my $shlib_wrap = bldtop_file("util", "shlib_wrap.sh");
my $apps_openssl = bldtop_file("apps", "openssl");
my $cert = srctop_file("apps", "server.pem");

sub run_test {
    my $tfo = shift;

    my $client_good = ! $tfo;
    my $server_good = ! $tfo;
    my $connect_good = 0;
    my $port = "0";

    # Not using TLSv1.3 allows the test to work with "no-ec"
    my @s_cmd = ("s_server", "-accept", ":0", "-cert", $cert, "-www", "-no_tls1_3", "-naccept", "1");
    push @s_cmd, "-tfo" if ($tfo);

    my $spid = open2(my $sout, my $sin, $shlib_wrap, $apps_openssl, @s_cmd);

    # Read until we get the port, TFO is output before the ACCEPT line
    while (<$sout>) {
        chomp;
        $server_good = $tfo if /^Listening for TFO$/;
        if (/^ACCEPT\s.*:(\d+)$/) {
            $port = $1;
            last;
        }
    }
    print STDERR "Port: $port\n";
    print STDERR "Invalid port\n" if ! ok($port);

    # Start up the client
    my @c_cmd = ("s_client", "-connect", ":$port", "-no_tls1_3");
    push @c_cmd, "-tfo" if ($tfo);

    my $cpid = open2(my $cout, my $cin, $shlib_wrap, $apps_openssl, @c_cmd);

    # Do the "GET", which will cause the client to finish
    print $cin "GET /\r\n";

    waitpid($cpid, 0);
    waitpid($spid, 0);

    # Check the client output
    while (<$cout>) {
        chomp;
        $client_good = $tfo if /^Connecting via TFO$/;
        $connect_good = 1 if /^Content-type: text/;
    }

    print STDERR "Client TFO check failed\n" if ! ok($client_good);
    print STDERR "Server TFO check failed\n" if ! ok($server_good);
    print STDERR "Connection failed\n" if ! ok($connect_good);
}

for my $tfo (0..1) {
    SKIP:
    {
        skip "TFO not enabled", 4 if disabled("tfo") && $tfo;

        run_test($tfo);
    }
}
