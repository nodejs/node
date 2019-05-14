#! /usr/bin/env perl
# Copyright 2016-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;
use TLSProxy::Proxy;

my $test_name = "test_renegotiation";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLS <= 1.2 enabled"
    if alldisabled(("ssl3", "tls1", "tls1_1", "tls1_2"));

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';
my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

#Test 1: A basic renegotiation test
$proxy->clientflags("-no_tls1_3");
$proxy->reneg(1);
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 3;
ok(TLSProxy::Message->success(), "Basic renegotiation");

#Test 2: Client does not send the Reneg SCSV. Reneg should fail
$proxy->clear();
$proxy->filter(\&reneg_filter);
$proxy->clientflags("-no_tls1_3");
$proxy->reneg(1);
$proxy->start();
ok(TLSProxy::Message->fail(), "No client SCSV");

SKIP: {
    skip "TLSv1.2 or TLSv1.1 disabled", 1
        if disabled("tls1_2") || disabled("tls1_1");
    #Test 3: Check that the ClientHello version remains the same in the reneg
    #        handshake
    $proxy->clear();
    $proxy->filter(undef);
    $proxy->clientflags("-no_tls1_3");
    $proxy->serverflags("-no_tls1_3 -no_tls1_2");
    $proxy->reneg(1);
    $proxy->start();
    my $chversion;
    my $chmatch = 0;
    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_CLIENT_HELLO) {
            if (!defined $chversion) {
                $chversion = $message->client_version;
            } else {
                if ($chversion == $message->client_version) {
                    $chmatch = 1;
                }
            }
        }
    }
    ok(TLSProxy::Message->success() && $chmatch,
       "Check ClientHello version is the same");
}

sub reneg_filter
{
    my $proxy = shift;

    # We're only interested in the initial ClientHello message
    if ($proxy->flight != 0) {
        return;
    }

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_CLIENT_HELLO) {
            #Remove any SCSV ciphersuites - just leave AES128-SHA (0x002f)
            my @ciphersuite = (0x002f);
            $message->ciphersuites(\@ciphersuite);
            $message->ciphersuite_len(2);
            $message->repack();
        }
    }
}
