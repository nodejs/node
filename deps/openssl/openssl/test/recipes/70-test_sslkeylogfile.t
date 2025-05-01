#! /usr/bin/env perl
# Copyright 2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use IPC::Open3;
use OpenSSL::Test qw/:DEFAULT result_dir srctop_file bldtop_file/;
use OpenSSL::Test::Utils;

my $test_name = "test_sslkeylogfile";
setup($test_name);

plan skip_all => "$test_name requires SSLKEYLOGFILE support"
    if disabled("sslkeylog");

plan tests => 1;


my $shlib_wrap   = srctop_file("util", "wrap.pl");
my $apps_openssl = srctop_file("apps", "openssl");
my $server_pem   = srctop_file("test", "certs", "servercert.pem");
my $server_key   = srctop_file("test", "certs", "serverkey.pem");

my $resultdir = result_dir();
my $sslkeylogfile = "$resultdir/sslkeylog.keys";
my $trace_file = "$resultdir/keylog.keys";

# Start s_server
my @s_server_cmd = ("s_server", "-accept", "0", "-naccept", "1",
                    "-cert", $server_pem, "-key", $server_key);
my $s_server_pid = open3(my $s_server_i, my $s_server_o, my $s_server_e, $shlib_wrap, $apps_openssl, @s_server_cmd);

# expected outputs from the server
# ACCEPT 0.0.0.0:<port>
# ACCEPT [::]:<port>
my $port = "0";
# Figure out what port its listening on
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
my $server_port = $port;

print("s_server ready, listening on port $server_port\n");

# Use SSLKEYLOGFILE to record keylogging
$ENV{SSLKEYLOGFILE} = $sslkeylogfile; 

# Start a client and use the -keylogfile option to independently trace keylog messages
my @s_client_cmd = ("s_client", "-connect", "localhost:$server_port", "-keylogfile", $trace_file);
my $s_client_pid = open3(my $s_client_i, my $s_client_o, my $s_client_e, $shlib_wrap, $apps_openssl, @s_client_cmd);

# Issue a quit command to terminate the client after connect
print $s_client_i "Q\n";
waitpid($s_client_pid, 0);
kill 'HUP', $s_server_pid;

# Test 1: Compare the output of -keylogfile  and SSLKEYLOGFILE, and make sure they match
# Note, the former adds a comment, that the latter does not, so ignore comments with -I in diff
ok(run(cmd(["diff", "-I" ,"^#.*\$", $sslkeylogfile, $trace_file])));
