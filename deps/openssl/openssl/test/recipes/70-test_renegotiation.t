#! /usr/bin/env perl
# Copyright 2016-2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use List::Util 'first';
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

plan tests => 9;

my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

#Test 1: A basic renegotiation test
$proxy->clientflags("-no_tls1_3");
$proxy->serverflags("-client_renegotiation");
$proxy->reneg(1);
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
ok(TLSProxy::Message->success(), "Basic renegotiation");

#Test 2: Seclevel 0 client does not send the Reneg SCSV. Reneg should fail
$proxy->clear();
$proxy->filter(\&reneg_scsv_filter);
$proxy->cipherc("DEFAULT:\@SECLEVEL=0");
$proxy->clientflags("-no_tls1_3");
$proxy->serverflags("-client_renegotiation");
$proxy->reneg(1);
$proxy->start();
ok(TLSProxy::Message->fail(), "No client SCSV");

SKIP: {
    skip "TLSv1.2 disabled", 1
        if disabled("tls1_2");

    #Test 3: TLS 1.2 client does not send the Reneg extension. Reneg should fail

    $proxy->clear();
    $proxy->cipherc("DEFAULT:\@SECLEVEL=2");
    $proxy->filter(\&reneg_ext_filter);
    $proxy->clientflags("-no_tls1_3");
    $proxy->serverflags("-client_renegotiation");
    $proxy->reneg(1);
    $proxy->start();
    ok(TLSProxy::Message->fail(), "No client extension");
}

SKIP: {
    skip "TLSv1.2 or TLSv1.1 disabled", 1
        if disabled("tls1_2") || disabled("tls1_1");
    #Test 4: Check that the ClientHello version remains the same in the reneg
    #        handshake
    $proxy->clear();
    $proxy->filter(undef);
    $proxy->ciphers("DEFAULT:\@SECLEVEL=0");
    $proxy->clientflags("-no_tls1_3 -cipher AES128-SHA:\@SECLEVEL=0");
    $proxy->serverflags("-no_tls1_3 -no_tls1_2 -client_renegotiation");
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

SKIP: {
    skip "TLSv1.2 disabled", 1
        if disabled("tls1_2");

    #Test 5: Test for CVE-2021-3449. client_sig_algs instead of sig_algs in
    #        resumption ClientHello
    $proxy->clear();
    $proxy->filter(\&sigalgs_filter);
    $proxy->clientflags("-tls1_2");
    $proxy->serverflags("-client_renegotiation");
    $proxy->reneg(1);
    $proxy->start();
    ok(TLSProxy::Message->fail(), "client_sig_algs instead of sig_algs");
}

SKIP: {
    skip "TLSv1.2 and TLSv1.1 disabled", 1
        if disabled("tls1_2") && disabled("tls1_1");
    #Test 6: Client fails to do renegotiation
    $proxy->clear();
    $proxy->filter(undef);
    $proxy->serverflags("-no_tls1_3");
    $proxy->clientflags("-no_tls1_3");
    $proxy->reneg(1);
    $proxy->start();
    ok(TLSProxy::Message->fail(),
        "Check client renegotiation failed");
}

SKIP: {
    skip "TLSv1 disabled", 1
        if disabled("tls1");

    #Test 7: Check that SECLEVEL 0 sends SCSV not RI extension
    $proxy->clear();
    $proxy->filter(undef);
    $proxy->cipherc("DEFAULT:\@SECLEVEL=0");
    $proxy->start();

    my $clientHello = first { $_->mt == TLSProxy::Message::MT_CLIENT_HELLO } @{$proxy->message_list};
    my $has_scsv = 255 ~~ @{$clientHello->ciphersuites};
    my $has_ri_extension = exists $clientHello->extension_data()->{TLSProxy::Message::EXT_RENEGOTIATE};

    ok($has_scsv && !$has_ri_extension, "SECLEVEL=0 should use SCSV not RI extension by default");
}

SKIP: {
    skip "TLSv1.2 disabled", 1
        if disabled("tls1_2");

    #Test 8: Check that SECLEVEL0 + TLS 1.2 sends RI extension not SCSV
    $proxy->clear();
    $proxy->filter(undef);
    $proxy->cipherc("DEFAULT:\@SECLEVEL=0");
    $proxy->clientflags("-tls1_2");
    $proxy->start();

    my $clientHello = first { $_->mt == TLSProxy::Message::MT_CLIENT_HELLO } @{$proxy->message_list};
    my $has_scsv = 255 ~~ @{$clientHello->ciphersuites};
    my $has_ri_extension = exists $clientHello->extension_data()->{TLSProxy::Message::EXT_RENEGOTIATE};

    ok(!$has_scsv && $has_ri_extension, "TLS1.2 should use RI extension despite SECLEVEL=0");
}


SKIP: {
    skip "TLSv1.3 disabled", 1
        if disabled("tls1_3");

    #Test 9: Check that TLS 1.3 sends neither RI extension nor SCSV
    $proxy->clear();
    $proxy->filter(undef);
    $proxy->clientflags("-tls1_3");
    $proxy->start();

    my $clientHello = first { $_->mt == TLSProxy::Message::MT_CLIENT_HELLO } @{$proxy->message_list};
    my $has_scsv = 255 ~~ @{$clientHello->ciphersuites};
    my $has_ri_extension = exists $clientHello->extension_data()->{TLSProxy::Message::EXT_RENEGOTIATE};

    ok(!$has_scsv && !$has_ri_extension, "TLS1.3 should not use RI extension or SCSV");
}

sub reneg_scsv_filter
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

sub reneg_ext_filter
{
    my $proxy = shift;

    # We're only interested in the initial ClientHello message
    if ($proxy->flight != 0) {
        return;
    }

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_CLIENT_HELLO) {
            $message->delete_extension(TLSProxy::Message::EXT_RENEGOTIATE);
            $message->repack();
        }
    }
}

sub sigalgs_filter
{
    my $proxy = shift;
    my $cnt = 0;

    # We're only interested in the second ClientHello message
    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_CLIENT_HELLO) {
            next if ($cnt++ == 0);

            my $sigs = pack "C10", 0x00, 0x08,
                            # rsa_pkcs_sha{256,384,512,1}
                            0x04, 0x01,  0x05, 0x01,  0x06, 0x01,  0x02, 0x01;
            $message->set_extension(TLSProxy::Message::EXT_SIG_ALGS_CERT, $sigs);
            $message->delete_extension(TLSProxy::Message::EXT_SIG_ALGS);
            $message->repack();
        }
    }
}
