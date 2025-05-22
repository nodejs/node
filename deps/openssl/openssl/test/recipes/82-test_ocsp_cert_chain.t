#! /usr/bin/env perl
# Copyright 2023-2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use IPC::Open3;
use OpenSSL::Test qw/:DEFAULT srctop_file bldtop_file/;
use OpenSSL::Test::Utils;
use Symbol 'gensym';

my $test_name = "test_ocsp_cert_chain";
setup($test_name);

plan skip_all => "$test_name requires OCSP support"
    if disabled("ocsp");
plan skip_all => "$test_name requires EC cryptography"
    if disabled("ec");
plan skip_all => "$test_name requires sock enabled"
    if disabled("sock");
plan skip_all => "$test_name requires TLS enabled"
    if alldisabled(available_protocols("tls"));
plan skip_all => "$test_name is not available Windows or VMS"
    if $^O =~ /^(VMS|MSWin32|msys)$/;

plan tests => 3;

my $shlib_wrap   = bldtop_file("util", "shlib_wrap.sh");
my $apps_openssl = bldtop_file("apps", "openssl");

my $index_txt             = srctop_file("test", "ocsp-tests", "index.txt");
my $ocsp_pem              = srctop_file("test", "ocsp-tests", "ocsp.pem");
my $intermediate_cert_pem = srctop_file("test", "ocsp-tests", "intermediate-cert.pem");

my $server_pem            = srctop_file("test", "ocsp-tests", "server.pem");

sub run_test {

    # this test starts two servers that listen on respective ports.
    # that can be problematic since the ports may not be available
    # (e.g. when multiple instances of the test are run on the same
    # machine).

    # to avoid this, we specify port 0 when staring each server, which
    # causes the OS to provide a random unused port.

    # using a random port with s_server is straightforward.  doing so
    # with the ocsp responder required some investigation because the
    # url for the ocsp responder is usually included in the server's
    # cert (normally, in the authority-information-access extension,
    # and it would be complicated to change that when the test
    # executes).  however, s_server has an option "-status_url" that
    # can be used to specify a fallback url when no url is specified
    # in the cert.  that is what we do here.

    # openssl ocsp -port 0 -index index.txt -rsigner ocsp.pem -CA intermediate-cert.pem
    my @ocsp_cmd = ("ocsp", "-port", "0", "-index", $index_txt, "-rsigner", $ocsp_pem, "-CA", $intermediate_cert_pem);
    my $ocsp_pid = open3(my $ocsp_i, my $ocsp_o, my $ocsp_e = gensym, $shlib_wrap, $apps_openssl, @ocsp_cmd);

    ## ipv4
    # ACCEPT 0.0.0.0:19254 PID=620007
    ## ipv6
    # ACCEPT [::]:19254 PID=620007
    my $port = "0";
    while (<$ocsp_o>) {
        print($_);
        chomp;
        if (/^ACCEPT 0.0.0.0:(\d+)/) {
            $port = $1;
            last;
        } elsif (/^ACCEPT \[::\]:(\d+)/) {
            $port = $1;
            last;
        } else {
            last;
        }
    }
    ok($port ne "0", "ocsp server port check");
    my $ocsp_port = $port;

    print("ocsp server ready, listening on port $ocsp_port\n");

    # openssl s_server -accept 0 -naccept 1 \
    #                  -cert server.pem -cert_chain intermediate-cert.pem \
    #                  -status_verbose -status_url http://localhost:19254/ocsp
    my @s_server_cmd = ("s_server", "-accept", "0", "-naccept", "1",
                        "-cert", $server_pem, "-cert_chain", $intermediate_cert_pem,
                        "-status_verbose", "-status_url", "http://localhost:${ocsp_port}/ocsp");
    my $s_server_pid = open3(my $s_server_i, my $s_server_o, my $s_server_e = gensym, $shlib_wrap, $apps_openssl, @s_server_cmd);

    # ACCEPT 0.0.0.0:45921
    # ACCEPT [::]:45921
    $port = "0";
    while (<$s_server_o>) {
        print($_);
        chomp;
        if (/^ACCEPT 0.0.0.0:(\d+)/) {
            $port = $1;
            last;
        } elsif (/^ACCEPT \[::\]:(\d+)/) {
            $port = $1;
            last;
        } elsif (/^Using default/) {
            ;
        } else {
            last;
        }
    }
    ok($port ne "0", "s_server port check");
    my $server_port = $port;

    print("s_server ready, listening on port $server_port\n");

    # openssl s_client -connect localhost:45921 -status -verify_return_error
    my @s_client_cmd = ("s_client", "-connect", "localhost:$server_port", "-status", "-verify_return_error");
    my $s_client_pid = open3(my $s_client_i, my $s_client_o, my $s_client_e = gensym, $shlib_wrap, $apps_openssl, @s_client_cmd);

    waitpid($s_client_pid, 0);
    kill 'HUP', $s_server_pid, $ocsp_pid;

    ### the output from s_server that we want to check is written to its stderr
    ###    cert_status: ocsp response sent:

    my $resp = 0;
    while (<$s_server_e>) {
        print($_);
        chomp;
        if (/^cert_status: ocsp response sent:/) {
            $resp = 1;
            last;
        }
    }
    ok($resp == 1, "check s_server sent ocsp response");
}

run_test();
