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

my $test_name = "test_sslsignature";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLS enabled"
    if alldisabled(available_protocols("tls"));

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';
my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

use constant {
    NO_CORRUPTION => 0,
    CORRUPT_SERVER_CERT_VERIFY => 1,
    CORRUPT_CLIENT_CERT_VERIFY => 2,
    CORRUPT_TLS1_2_SERVER_KEY_EXCHANGE => 3,
};

$proxy->filter(\&signature_filter);

#Test 1: No corruption should succeed
my $testtype = NO_CORRUPTION;
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 4;
ok(TLSProxy::Message->success, "No corruption");

SKIP: {
    skip "TLSv1.3 disabled", 1 if disabled("tls1_3");

    #Test 2: Corrupting a server CertVerify signature in TLSv1.3 should fail
    $proxy->clear();
    $testtype = CORRUPT_SERVER_CERT_VERIFY;
    $proxy->start();
    ok(TLSProxy::Message->fail, "Corrupt server TLSv1.3 CertVerify");

    #Test x: Corrupting a client CertVerify signature in TLSv1.3 should fail
    #$proxy->clear();
    #$testtype = CORRUPT_CLIENT_CERT_VERIFY;
    #$proxy->serverflags("-Verify 5");
    #$proxy->clientflags("-cert ".srctop_file("apps", "server.pem"));
    #$proxy->start();
    #ok(TLSProxy::Message->fail, "Corrupt client TLSv1.3 CertVerify");
    #TODO(TLS1.3): This test fails due to a problem in s_server/TLSProxy.
    #Currently a connection is counted as "successful" if the client ends it
    #with a close_notify. In TLSProxy the client initiates the closure of the
    #connection so really we should not count it as successful until s_server
    #has also responded with a close_notify. However s_server never sends a
    #close_notify - it just closes the connection. Fixing this would be a
    #significant change to the long established behaviour of s_server.
    #Unfortunately in this test, it is the server that notices the incorrect
    #signature and responds with an appropriate alert. However s_client never
    #sees that because it occurs after the server Finished has been sent.
    #Therefore s_client just continues to send its application data and sends
    #its close_notify regardless. TLSProxy sees this and thinks that the
    #connection was successful when in fact it was not. There isn't an easy fix
    #for this, so leaving this test commented out for now.
}

SKIP: {
    skip "TLS <= 1.2 disabled", 2
        if alldisabled(("ssl3", "tls1", "tls1_1", "tls1_2"));

    #Test 3: Corrupting a CertVerify signature in <=TLSv1.2 should fail
    $proxy->clear();
    $testtype = CORRUPT_CLIENT_CERT_VERIFY;
    $proxy->serverflags("-Verify 5");
    $proxy->clientflags("-no_tls1_3 -cert ".srctop_file("apps", "server.pem"));
    $proxy->start();
    ok(TLSProxy::Message->fail, "Corrupt <=TLSv1.2 CertVerify");

    SKIP: {
        skip "DH disabled", 1 if disabled("dh");

        #Test 4: Corrupting a ServerKeyExchange signature in <=TLSv1.2 should
        #fail
        $proxy->clear();
        $testtype = CORRUPT_TLS1_2_SERVER_KEY_EXCHANGE;
        $proxy->clientflags("-no_tls1_3");
        $proxy->cipherc('DHE-RSA-AES128-SHA');
        $proxy->ciphers('DHE-RSA-AES128-SHA');
        $proxy->start();
        ok(TLSProxy::Message->fail, "Corrupt <=TLSv1.2 ServerKeyExchange");
    }
}

sub signature_filter
{
    my $proxy = shift;
    my $flight;
    my $mt = TLSProxy::Message::MT_CERTIFICATE_VERIFY;

    if ($testtype == CORRUPT_SERVER_CERT_VERIFY
            || $testtype == CORRUPT_TLS1_2_SERVER_KEY_EXCHANGE
            || (!disabled("tls1_3") && $testtype == NO_CORRUPTION)) {
        $flight = 1;
    } else {
        $flight = 2;
    }

    # We're only interested in the initial server flight
    return if ($proxy->flight != $flight);

    $mt = TLSProxy::Message::MT_SERVER_KEY_EXCHANGE
        if ($testtype == CORRUPT_TLS1_2_SERVER_KEY_EXCHANGE);

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == $mt) {
            my $sig = $message->signature();
            my $sigbase = substr($sig, 0, -1);
            my $sigend = unpack("C", substr($sig, -1));

            #Flip bits in final byte of signature to corrupt the sig
            $sigend ^= 0xff unless $testtype == NO_CORRUPTION;

            $message->signature($sigbase.pack("C", $sigend));
            $message->repack();
        }
    }
}
