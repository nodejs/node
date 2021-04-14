#! /usr/bin/env perl
# Copyright 2017-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file srctop_dir bldtop_dir/;
use OpenSSL::Test::Utils;
use File::Temp qw(tempfile);
use TLSProxy::Proxy;
use checkhandshake qw(checkhandshake @handmessages @extensions);

my $test_name = "test_tls13kexmodes";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLSv1.3 enabled"
    if disabled("tls1_3") || (disabled("ec") && disabled("dh"));

plan skip_all => "$test_name needs EC enabled"
    if disabled("ec");

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';


@handmessages = (
    [TLSProxy::Message::MT_CLIENT_HELLO,
        checkhandshake::ALL_HANDSHAKES],
    [TLSProxy::Message::MT_SERVER_HELLO,
        checkhandshake::HRR_HANDSHAKE | checkhandshake::HRR_RESUME_HANDSHAKE],
    [TLSProxy::Message::MT_CLIENT_HELLO,
        checkhandshake::HRR_HANDSHAKE | checkhandshake::HRR_RESUME_HANDSHAKE],
    [TLSProxy::Message::MT_SERVER_HELLO,
        checkhandshake::ALL_HANDSHAKES],
    [TLSProxy::Message::MT_ENCRYPTED_EXTENSIONS,
        checkhandshake::ALL_HANDSHAKES],
    [TLSProxy::Message::MT_CERTIFICATE_REQUEST,
        checkhandshake::CLIENT_AUTH_HANDSHAKE],
    [TLSProxy::Message::MT_CERTIFICATE,
        checkhandshake::ALL_HANDSHAKES & ~(checkhandshake::RESUME_HANDSHAKE | checkhandshake::HRR_RESUME_HANDSHAKE)],
    [TLSProxy::Message::MT_CERTIFICATE_VERIFY,
        checkhandshake::ALL_HANDSHAKES & ~(checkhandshake::RESUME_HANDSHAKE | checkhandshake::HRR_RESUME_HANDSHAKE)],
    [TLSProxy::Message::MT_FINISHED,
        checkhandshake::ALL_HANDSHAKES],
    [TLSProxy::Message::MT_CERTIFICATE,
        checkhandshake::CLIENT_AUTH_HANDSHAKE],
    [TLSProxy::Message::MT_CERTIFICATE_VERIFY,
        checkhandshake::CLIENT_AUTH_HANDSHAKE],
    [TLSProxy::Message::MT_FINISHED,
        checkhandshake::ALL_HANDSHAKES],
    [0, 0]
);

@extensions = (
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SERVER_NAME,
        TLSProxy::Message::CLIENT,
        checkhandshake::SERVER_NAME_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_STATUS_REQUEST,
        TLSProxy::Message::CLIENT,
        checkhandshake::STATUS_REQUEST_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SUPPORTED_GROUPS,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_EC_POINT_FORMATS,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SIG_ALGS,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_ALPN,
        TLSProxy::Message::CLIENT,
        checkhandshake::ALPN_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SCT,
        TLSProxy::Message::CLIENT,
        checkhandshake::SCT_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_ENCRYPT_THEN_MAC,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_EXTENDED_MASTER_SECRET,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SESSION_TICKET,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_KEY_SHARE,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SUPPORTED_VERSIONS,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_PSK_KEX_MODES,
        TLSProxy::Message::CLIENT,
        checkhandshake::PSK_KEX_MODES_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_PSK,
        TLSProxy::Message::CLIENT,
        checkhandshake::PSK_CLI_EXTENSION],

    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_SUPPORTED_VERSIONS,
        TLSProxy::Message::SERVER,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_KEY_SHARE,
        TLSProxy::Message::SERVER,
        checkhandshake::KEY_SHARE_HRR_EXTENSION],

    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SERVER_NAME,
        TLSProxy::Message::CLIENT,
        checkhandshake::SERVER_NAME_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_STATUS_REQUEST,
        TLSProxy::Message::CLIENT,
        checkhandshake::STATUS_REQUEST_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SUPPORTED_GROUPS,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_EC_POINT_FORMATS,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SIG_ALGS,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_ALPN,
        TLSProxy::Message::CLIENT,
        checkhandshake::ALPN_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SCT,
        TLSProxy::Message::CLIENT,
        checkhandshake::SCT_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_ENCRYPT_THEN_MAC,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_EXTENDED_MASTER_SECRET,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SESSION_TICKET,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_KEY_SHARE,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SUPPORTED_VERSIONS,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_PSK_KEX_MODES,
        TLSProxy::Message::CLIENT,
        checkhandshake::PSK_KEX_MODES_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_PSK,
        TLSProxy::Message::CLIENT,
        checkhandshake::PSK_CLI_EXTENSION],

    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_SUPPORTED_VERSIONS,
        TLSProxy::Message::SERVER,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_KEY_SHARE,
        TLSProxy::Message::SERVER,
        checkhandshake::KEY_SHARE_SRV_EXTENSION],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_PSK,
        TLSProxy::Message::SERVER,
        checkhandshake::PSK_SRV_EXTENSION],

    [TLSProxy::Message::MT_CERTIFICATE, TLSProxy::Message::EXT_STATUS_REQUEST,
        TLSProxy::Message::SERVER,
        checkhandshake::STATUS_REQUEST_SRV_EXTENSION],
    [0,0,0,0]
);

use constant {
    DELETE_EXTENSION => 0,
    EMPTY_EXTENSION => 1,
    NON_DHE_KEX_MODE_ONLY => 2,
    DHE_KEX_MODE_ONLY => 3,
    UNKNOWN_KEX_MODES => 4,
    BOTH_KEX_MODES => 5
};

my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

#Test 1: First get a session
(undef, my $session) = tempfile();
$proxy->clientflags("-sess_out ".$session);
$proxy->serverflags("-servername localhost");
$proxy->sessionfile($session);
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 11;
ok(TLSProxy::Message->success(), "Initial connection");

#Test 2: Attempt a resume with no kex modes extension. Should fail (server
#        MUST abort handshake with pre_shared key and no psk_kex_modes)
$proxy->clear();
$proxy->clientflags("-sess_in ".$session);
my $testtype = DELETE_EXTENSION;
$proxy->filter(\&modify_kex_modes_filter);
$proxy->start();
ok(TLSProxy::Message->fail(), "Resume with no kex modes");

#Test 3: Attempt a resume with empty kex modes extension. Should fail (empty
#        extension is invalid)
$proxy->clear();
$proxy->clientflags("-sess_in ".$session);
$testtype = EMPTY_EXTENSION;
$proxy->start();
ok(TLSProxy::Message->fail(), "Resume with empty kex modes");

#Test 4: Attempt a resume with non-dhe kex mode only. Should resume without a
#        key_share
$proxy->clear();
$proxy->clientflags("-allow_no_dhe_kex -sess_in ".$session);
$proxy->serverflags("-allow_no_dhe_kex");
$testtype = NON_DHE_KEX_MODE_ONLY;
$proxy->start();
checkhandshake($proxy, checkhandshake::RESUME_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::PSK_KEX_MODES_EXTENSION
               | checkhandshake::PSK_CLI_EXTENSION
               | checkhandshake::PSK_SRV_EXTENSION,
               "Resume with non-dhe kex mode");

#Test 5: Attempt a resume with dhe kex mode only. Should resume with a key_share
$proxy->clear();
$proxy->clientflags("-sess_in ".$session);
$testtype = DHE_KEX_MODE_ONLY;
$proxy->start();
checkhandshake($proxy, checkhandshake::RESUME_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::PSK_KEX_MODES_EXTENSION
               | checkhandshake::KEY_SHARE_SRV_EXTENSION
               | checkhandshake::PSK_CLI_EXTENSION
               | checkhandshake::PSK_SRV_EXTENSION,
               "Resume with non-dhe kex mode");

#Test 6: Attempt a resume with only unrecognised kex modes. Should not resume
#        but rather fall back to full handshake
$proxy->clear();
$proxy->clientflags("-sess_in ".$session);
$testtype = UNKNOWN_KEX_MODES;
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::PSK_KEX_MODES_EXTENSION
               | checkhandshake::KEY_SHARE_SRV_EXTENSION
               | checkhandshake::PSK_CLI_EXTENSION,
               "Resume with unrecognized kex mode");

#Test 7: Attempt a resume with both non-dhe and dhe kex mode. Should resume with
#        a key_share
$proxy->clear();
$proxy->clientflags("-sess_in ".$session);
$testtype = BOTH_KEX_MODES;
$proxy->start();
checkhandshake($proxy, checkhandshake::RESUME_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::PSK_KEX_MODES_EXTENSION
               | checkhandshake::KEY_SHARE_SRV_EXTENSION
               | checkhandshake::PSK_CLI_EXTENSION
               | checkhandshake::PSK_SRV_EXTENSION,
               "Resume with non-dhe kex mode");

#Test 8: Attempt a resume with both non-dhe and dhe kex mode, but unacceptable
#        initial key_share. Should resume with a key_share following an HRR
$proxy->clear();
$proxy->clientflags("-sess_in ".$session);
$proxy->serverflags("-curves P-256");
$testtype = BOTH_KEX_MODES;
$proxy->start();
checkhandshake($proxy, checkhandshake::HRR_RESUME_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::PSK_KEX_MODES_EXTENSION
               | checkhandshake::KEY_SHARE_SRV_EXTENSION
               | checkhandshake::KEY_SHARE_HRR_EXTENSION
               | checkhandshake::PSK_CLI_EXTENSION
               | checkhandshake::PSK_SRV_EXTENSION,
               "Resume with both kex modes and HRR");

#Test 9: Attempt a resume with dhe kex mode only and an unacceptable initial
#        key_share. Should resume with a key_share following an HRR
$proxy->clear();
$proxy->clientflags("-sess_in ".$session);
$proxy->serverflags("-curves P-256");
$testtype = DHE_KEX_MODE_ONLY;
$proxy->start();
checkhandshake($proxy, checkhandshake::HRR_RESUME_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::PSK_KEX_MODES_EXTENSION
               | checkhandshake::KEY_SHARE_SRV_EXTENSION
               | checkhandshake::KEY_SHARE_HRR_EXTENSION
               | checkhandshake::PSK_CLI_EXTENSION
               | checkhandshake::PSK_SRV_EXTENSION,
               "Resume with dhe kex mode and HRR");

#Test 10: Attempt a resume with both non-dhe and dhe kex mode, unacceptable
#         initial key_share and no overlapping groups. Should resume without a
#         key_share
$proxy->clear();
$proxy->clientflags("-allow_no_dhe_kex -curves P-384 -sess_in ".$session);
$proxy->serverflags("-allow_no_dhe_kex -curves P-256");
$testtype = BOTH_KEX_MODES;
$proxy->start();
checkhandshake($proxy, checkhandshake::RESUME_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::PSK_KEX_MODES_EXTENSION
               | checkhandshake::PSK_CLI_EXTENSION
               | checkhandshake::PSK_SRV_EXTENSION,
               "Resume with both kex modes, no overlapping groups");

#Test 11: Attempt a resume with dhe kex mode only, unacceptable
#         initial key_share and no overlapping groups. Should fail
$proxy->clear();
$proxy->clientflags("-curves P-384 -sess_in ".$session);
$proxy->serverflags("-curves P-256");
$testtype = DHE_KEX_MODE_ONLY;
$proxy->start();
ok(TLSProxy::Message->fail(), "Resume with dhe kex mode, no overlapping groups");

unlink $session;

sub modify_kex_modes_filter
{
    my $proxy = shift;

    # We're only interested in the initial ClientHello
    return if ($proxy->flight != 0);

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_CLIENT_HELLO) {
            my $ext;

            if ($testtype == EMPTY_EXTENSION) {
                $ext = pack "C",
                    0x00;       #List length
            } elsif ($testtype == NON_DHE_KEX_MODE_ONLY) {
                $ext = pack "C2",
                    0x01,       #List length
                    0x00;       #psk_ke
            } elsif ($testtype == DHE_KEX_MODE_ONLY) {
                $ext = pack "C2",
                    0x01,       #List length
                    0x01;       #psk_dhe_ke
            } elsif ($testtype == UNKNOWN_KEX_MODES) {
                $ext = pack "C3",
                    0x02,       #List length
                    0xfe,       #unknown
                    0xff;       #unknown
            } elsif ($testtype == BOTH_KEX_MODES) {
                #We deliberately list psk_ke first...should still use psk_dhe_ke
                $ext = pack "C3",
                    0x02,       #List length
                    0x00,       #psk_ke
                    0x01;       #psk_dhe_ke
            }

            if ($testtype == DELETE_EXTENSION) {
                $message->delete_extension(
                    TLSProxy::Message::EXT_PSK_KEX_MODES);
            } else {
                $message->set_extension(
                    TLSProxy::Message::EXT_PSK_KEX_MODES, $ext);
            }

            $message->repack();
        }
    }
}
