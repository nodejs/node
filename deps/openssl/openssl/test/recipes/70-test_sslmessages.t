#! /usr/bin/env perl
# Copyright 2015-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;
use File::Temp qw(tempfile);
use TLSProxy::Proxy;
my $test_name = "test_tls13messages";
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

use constant {
    DEFAULT_HANDSHAKE => 1,
    OCSP_HANDSHAKE => 2,
    RESUME_HANDSHAKE => 4,
    CLIENT_AUTH_HANDSHAKE => 8,
    RENEG_HANDSHAKE => 16,

    ALL_HANDSHAKES => 31
};

my @handmessages = (
    [TLSProxy::Message::MT_CLIENT_HELLO, ALL_HANDSHAKES],
    [TLSProxy::Message::MT_SERVER_HELLO, ALL_HANDSHAKES],
    [TLSProxy::Message::MT_CERTIFICATE, ALL_HANDSHAKES & ~RESUME_HANDSHAKE],
    [TLSProxy::Message::MT_CERTIFICATE_STATUS, OCSP_HANDSHAKE],
    #ServerKeyExchange handshakes not currently supported by TLSProxy
    [TLSProxy::Message::MT_CERTIFICATE_REQUEST, CLIENT_AUTH_HANDSHAKE],
    [TLSProxy::Message::MT_SERVER_HELLO_DONE, ALL_HANDSHAKES & ~RESUME_HANDSHAKE],
    [TLSProxy::Message::MT_CERTIFICATE, CLIENT_AUTH_HANDSHAKE],
    [TLSProxy::Message::MT_CLIENT_KEY_EXCHANGE, ALL_HANDSHAKES & ~RESUME_HANDSHAKE],
    [TLSProxy::Message::MT_CERTIFICATE_VERIFY, CLIENT_AUTH_HANDSHAKE],
    [TLSProxy::Message::MT_FINISHED, ALL_HANDSHAKES],
    [TLSProxy::Message::MT_NEW_SESSION_TICKET, ALL_HANDSHAKES & ~RESUME_HANDSHAKE],
    [TLSProxy::Message::MT_FINISHED, ALL_HANDSHAKES],
    [TLSProxy::Message::MT_CLIENT_HELLO, RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_SERVER_HELLO, RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_CERTIFICATE, RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_SERVER_HELLO_DONE, RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_CLIENT_KEY_EXCHANGE, RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_FINISHED, RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_NEW_SESSION_TICKET, RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_FINISHED, RENEG_HANDSHAKE],
    [0, 0]
);

my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

sub checkmessages($$);

#Test 1: Check we get all the right messages for a default handshake
(undef, my $session) = tempfile();
$proxy->serverconnects(2);
$proxy->clientflags("-sess_out ".$session);
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 5;
checkmessages(DEFAULT_HANDSHAKE, "Default handshake test");

#Test 2: Resumption handshake
$proxy->clearClient();
$proxy->clientflags("-sess_in ".$session);
$proxy->clientstart();
checkmessages(RESUME_HANDSHAKE, "Resumption handshake test");
unlink $session;

#Test 3: A client auth handshake
$proxy->clear();
$proxy->clientflags("-cert ".srctop_file("apps", "server.pem"));
$proxy->serverflags("-Verify 5");
$proxy->start();
checkmessages(CLIENT_AUTH_HANDSHAKE, "Client auth handshake test");

#Test 4: A handshake with a renegotiation
$proxy->clear();
$proxy->reneg(1);
$proxy->start();
checkmessages(RENEG_HANDSHAKE, "Renegotiation handshake test");

#Test 5: A handshake with a renegotiation and client auth
$proxy->clear();
$proxy->clientflags("-cert ".srctop_file("apps", "server.pem"));
$proxy->serverflags("-Verify 5");
$proxy->reneg(1);
$proxy->start();
checkmessages(RENEG_HANDSHAKE | CLIENT_AUTH_HANDSHAKE,
              "Renogitation and client auth handshake test");

sub checkmessages($$)
{
    my ($handtype, $testname) = @_;

    subtest $testname => sub {
        my $loop = 0;
        my $numtests;

        #First count the number of tests
        for ($numtests = 0; $handmessages[$loop][1] != 0; $loop++) {
            $numtests++ if (($handmessages[$loop][1] & $handtype) != 0);
        }

        plan tests => $numtests;

        my $nextmess = 0;
        my $message = undef;
        for ($loop = 0; $handmessages[$loop][1] != 0; $loop++) {
            next if (($handmessages[$loop][1] & $handtype) == 0);
            if (scalar @{$proxy->message_list} > $nextmess) {
                $message = ${$proxy->message_list}[$nextmess];
                $nextmess++;
            } else {
                $message = undef;
            }
            if (!defined $message) {
                fail("Message type check. Got nothing, expected "
                     .$handmessages[$loop][0]);
            } else {
                ok($message->mt == $handmessages[$loop][0],
                   "Message type check. Got ".$message->mt
                   .", expected ".$handmessages[$loop][0]);
            }
        }
    }
}
