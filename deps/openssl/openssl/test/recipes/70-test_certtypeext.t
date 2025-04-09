#! /usr/bin/env perl
# Copyright 2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;
use TLSProxy::Proxy;

my $test_name = "test_certtypeext";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLSv1.2 enabled"
    if disabled("tls1_2");

my $proxy = TLSProxy::Proxy->new(
    \&certtype_filter,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

use constant {
    SERVER_CERT_TYPE => 0,
    CLIENT_CERT_TYPE => 1,
    NO_CERT_TYPE => 2
};
my $testtype;

# Test 1: Just do a verify without cert type
$proxy->clear();
$proxy->clientflags("-tls1_2 -cert ".srctop_file("apps", "server.pem"));
$proxy->serverflags("-verify 4");
$testtype = NO_CERT_TYPE;
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 4;
ok(TLSProxy::Message->success, "Simple verify");

# Test 2: Set a bogus server cert type
$proxy->clear();
$proxy->serverflags("-enable_server_rpk");
$testtype = SERVER_CERT_TYPE;
$proxy->start();
ok(TLSProxy::Message->fail, "Unsupported server cert type");

# Test 3: Set a bogus client cert type
$proxy->clear();
$proxy->serverflags("-enable_client_rpk");
$testtype = CLIENT_CERT_TYPE;
$proxy->start();
ok(TLSProxy::Message->success, "Unsupported client cert type, no verify");

# Test 4: Set a bogus server cert type with verify
$proxy->clear();
$testtype = CLIENT_CERT_TYPE;
$proxy->clientflags("-tls1_2 -cert ".srctop_file("apps", "server.pem"));
$proxy->serverflags("-verify 4 -enable_client_rpk");
$proxy->start();
ok(TLSProxy::Message->fail, "Unsupported client cert type with verify");

sub certtype_filter
{
    my $proxy = shift;
    my $message;

    # We're only interested in the initial ClientHello
    return if $proxy->flight != 0;

    $message = ${$proxy->message_list}[0];

    # Add unsupported and bogus client and server cert type to the client hello.
    my $ct = pack "C5", 0x04, 0x01, 0x03, 0x55, 0x66;
    if ($testtype == CLIENT_CERT_TYPE) {
        print "SETTING CLIENT CERT TYPE\n";
        $message->set_extension(TLSProxy::Message::EXT_CLIENT_CERT_TYPE, $ct);
    }
    if ($testtype == SERVER_CERT_TYPE) {
        print "SETTING SERVER CERT TYPE\n";
        $message->set_extension(TLSProxy::Message::EXT_SERVER_CERT_TYPE, $ct);
    }

    $message->repack();
}
