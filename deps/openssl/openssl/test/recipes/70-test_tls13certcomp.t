#! /usr/bin/env perl
# Copyright 2015-2024 The OpenSSL Project Authors. All Rights Reserved.
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

my $test_name = "test_tls13certcomp";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLSv1.3 enabled"
    if disabled("tls1_3");

plan skip_all => "$test_name needs EC enabled"
    if disabled("ec");

plan skip_all => "$test_name needs compression and algorithms enabled"
    if disabled("comp") || (disabled("brotli") && disabled("zlib") && disabled("zstd"));

@handmessages = (
    [TLSProxy::Message::MT_CLIENT_HELLO,
        checkhandshake::ALL_HANDSHAKES],
    [TLSProxy::Message::MT_SERVER_HELLO,
        checkhandshake::ALL_HANDSHAKES],
    [TLSProxy::Message::MT_ENCRYPTED_EXTENSIONS,
        checkhandshake::ALL_HANDSHAKES],
    [TLSProxy::Message::MT_CERTIFICATE_REQUEST,
        checkhandshake::CERT_COMP_CLI_HANDSHAKE | checkhandshake::CERT_COMP_BOTH_HANDSHAKE],
    [TLSProxy::Message::MT_CERTIFICATE,
        checkhandshake::ALL_HANDSHAKES & ~(checkhandshake::CERT_COMP_SRV_HANDSHAKE | checkhandshake::CERT_COMP_BOTH_HANDSHAKE)],
    [TLSProxy::Message::MT_COMPRESSED_CERTIFICATE,
        checkhandshake::CERT_COMP_SRV_HANDSHAKE | checkhandshake::CERT_COMP_BOTH_HANDSHAKE],
    [TLSProxy::Message::MT_CERTIFICATE_VERIFY,
        checkhandshake::ALL_HANDSHAKES],
    [TLSProxy::Message::MT_FINISHED,
        checkhandshake::ALL_HANDSHAKES],
    [TLSProxy::Message::MT_COMPRESSED_CERTIFICATE,
        checkhandshake::CERT_COMP_CLI_HANDSHAKE | checkhandshake::CERT_COMP_BOTH_HANDSHAKE],
    [TLSProxy::Message::MT_CERTIFICATE_VERIFY,
        checkhandshake::CERT_COMP_CLI_HANDSHAKE | checkhandshake::CERT_COMP_BOTH_HANDSHAKE],
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
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_PSK,
        TLSProxy::Message::CLIENT,
        checkhandshake::PSK_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_POST_HANDSHAKE_AUTH,
        TLSProxy::Message::CLIENT,
        checkhandshake::POST_HANDSHAKE_AUTH_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_COMPRESS_CERTIFICATE,
        TLSProxy::Message::CLIENT,
        checkhandshake::CERT_COMP_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_RENEGOTIATE,
        TLSProxy::Message::CLIENT,
        checkhandshake::DEFAULT_EXTENSIONS],

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
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_PSK,
        TLSProxy::Message::CLIENT,
        checkhandshake::PSK_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_POST_HANDSHAKE_AUTH,
        TLSProxy::Message::CLIENT,
        checkhandshake::POST_HANDSHAKE_AUTH_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_COMPRESS_CERTIFICATE,
        TLSProxy::Message::CLIENT,
        checkhandshake::CERT_COMP_CLI_EXTENSION],

    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_SUPPORTED_VERSIONS,
        TLSProxy::Message::SERVER,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_KEY_SHARE,
        TLSProxy::Message::SERVER,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_PSK,
        TLSProxy::Message::SERVER,
        checkhandshake::PSK_SRV_EXTENSION],

    [TLSProxy::Message::MT_ENCRYPTED_EXTENSIONS, TLSProxy::Message::EXT_SERVER_NAME,
        TLSProxy::Message::SERVER,
        checkhandshake::SERVER_NAME_SRV_EXTENSION],
    [TLSProxy::Message::MT_ENCRYPTED_EXTENSIONS, TLSProxy::Message::EXT_ALPN,
        TLSProxy::Message::SERVER,
        checkhandshake::ALPN_SRV_EXTENSION],
    [TLSProxy::Message::MT_ENCRYPTED_EXTENSIONS, TLSProxy::Message::EXT_SUPPORTED_GROUPS,
        TLSProxy::Message::SERVER,
        checkhandshake::SUPPORTED_GROUPS_SRV_EXTENSION],

    [TLSProxy::Message::MT_CERTIFICATE_REQUEST, TLSProxy::Message::EXT_SIG_ALGS,
        TLSProxy::Message::SERVER,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CERTIFICATE_REQUEST, TLSProxy::Message::EXT_COMPRESS_CERTIFICATE,
        TLSProxy::Message::SERVER,
        checkhandshake::CERT_COMP_SRV_EXTENSION],

    [TLSProxy::Message::MT_CERTIFICATE, TLSProxy::Message::EXT_STATUS_REQUEST,
        TLSProxy::Message::SERVER,
        checkhandshake::STATUS_REQUEST_SRV_EXTENSION],
    [TLSProxy::Message::MT_CERTIFICATE, TLSProxy::Message::EXT_SCT,
        TLSProxy::Message::SERVER,
        checkhandshake::SCT_SRV_EXTENSION],

    [0,0,0,0]
);

my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);


#Test 1: Client sends cert comp, but no client auth
$proxy->serverconnects(2);
$proxy->clear();
$proxy->serverflags("-no_tx_cert_comp -no_rx_cert_comp");
# One final skip check
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 8;
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::CERT_COMP_CLI_EXTENSION,
               "Client supports certificate compression");

#Test 2: Server sends cert comp, no client auth
$proxy->clear();
$proxy->clientflags("-no_tx_cert_comp -no_rx_cert_comp");
$proxy->serverflags("-cert_comp");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::CERT_COMP_SRV_EXTENSION,
               "Server supports certificate compression, but no client auth");

#Test 3: Both send cert comp, no client auth
$proxy->clear();
$proxy->serverflags("-cert_comp");
$proxy->start();
checkhandshake($proxy, checkhandshake::CERT_COMP_SRV_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::CERT_COMP_CLI_EXTENSION
               | checkhandshake::CERT_COMP_SRV_EXTENSION,
               "Both support certificate compression, but no client auth");

#Test 4: Both send cert comp, with client auth
$proxy->clear();
$proxy->clientflags("-cert ".srctop_file("apps", "server.pem"));
$proxy->serverflags("-Verify 5 -cert_comp");
$proxy->start();
checkhandshake($proxy, checkhandshake::CERT_COMP_BOTH_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::CERT_COMP_CLI_EXTENSION
               | checkhandshake::CERT_COMP_SRV_EXTENSION,
               "Both support certificate compression, with client auth");

#Test 5: Client-to-server-only certificate compression, with client auth
$proxy->clear();
$proxy->clientflags("-no_rx_cert_comp -cert ".srctop_file("apps", "server.pem"));
$proxy->serverflags("-no_tx_cert_comp -Verify 5 -cert_comp");
$proxy->start();
checkhandshake($proxy, checkhandshake::CERT_COMP_CLI_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::CERT_COMP_SRV_EXTENSION,
               "Client-to-server-only certificate compression, with client auth");

#Test 6: Server-to-client-only certificate compression
$proxy->clear();
$proxy->clientflags("-no_tx_cert_comp");
$proxy->serverflags("-no_rx_cert_comp -cert_comp");
$proxy->start();
checkhandshake($proxy, checkhandshake::CERT_COMP_SRV_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::CERT_COMP_CLI_EXTENSION,
               "Server-to-client-only certificate compression");

#Test 7: Neither side wants to send a compressed cert, but will accept one
$proxy->clear();
$proxy->clientflags("-no_tx_cert_comp");
$proxy->serverflags("-no_tx_cert_comp -cert_comp");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::CERT_COMP_CLI_EXTENSION
               | checkhandshake::CERT_COMP_SRV_EXTENSION,
               "Accept but not send compressed certificates");

#Test 8: Neither side wants to receive a compressed cert, but will send one
$proxy->clear();
$proxy->clientflags("-no_rx_cert_comp");
$proxy->serverflags("-no_rx_cert_comp -cert_comp");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS,
               "Send but not accept compressed certificates");
