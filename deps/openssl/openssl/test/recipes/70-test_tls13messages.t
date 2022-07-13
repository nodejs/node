#! /usr/bin/env perl
# Copyright 2015-2020 The OpenSSL Project Authors. All Rights Reserved.
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

my $test_name = "test_tls13messages";
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
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_PSK,
        TLSProxy::Message::CLIENT,
        checkhandshake::PSK_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_POST_HANDSHAKE_AUTH,
        TLSProxy::Message::CLIENT,
        checkhandshake::POST_HANDSHAKE_AUTH_CLI_EXTENSION],

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

#Test 1: Check we get all the right messages for a default handshake
(undef, my $session) = tempfile();
$proxy->serverconnects(2);
$proxy->clientflags("-sess_out ".$session);
$proxy->sessionfile($session);
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 17;
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS,
               "Default handshake test");

#Test 2: Resumption handshake
$proxy->clearClient();
$proxy->clientflags("-sess_in ".$session);
$proxy->clientstart();
checkhandshake($proxy, checkhandshake::RESUME_HANDSHAKE,
               (checkhandshake::DEFAULT_EXTENSIONS
                | checkhandshake::PSK_CLI_EXTENSION
                | checkhandshake::PSK_SRV_EXTENSION),
               "Resumption handshake test");

SKIP: {
    skip "No OCSP support in this OpenSSL build", 4
        if disabled("ct") || disabled("ec") || disabled("ocsp");
    #Test 3: A status_request handshake (client request only)
    $proxy->clear();
    $proxy->clientflags("-status");
    $proxy->start();
    checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS
                   | checkhandshake::STATUS_REQUEST_CLI_EXTENSION,
                   "status_request handshake test (client)");

    #Test 4: A status_request handshake (server support only)
    $proxy->clear();
    $proxy->serverflags("-status_file "
                        .srctop_file("test", "recipes", "ocsp-response.der"));
    $proxy->start();
    checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS,
                   "status_request handshake test (server)");

    #Test 5: A status_request handshake (client and server)
    $proxy->clear();
    $proxy->clientflags("-status");
    $proxy->serverflags("-status_file "
                        .srctop_file("test", "recipes", "ocsp-response.der"));
    $proxy->start();
    checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS
                   | checkhandshake::STATUS_REQUEST_CLI_EXTENSION
                   | checkhandshake::STATUS_REQUEST_SRV_EXTENSION,
                   "status_request handshake test");

    #Test 6: A status_request handshake (client and server) with client auth
    $proxy->clear();
    $proxy->clientflags("-status -enable_pha -cert "
                        .srctop_file("apps", "server.pem"));
    $proxy->serverflags("-Verify 5 -status_file "
                        .srctop_file("test", "recipes", "ocsp-response.der"));
    $proxy->start();
    checkhandshake($proxy, checkhandshake::CLIENT_AUTH_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS
                   | checkhandshake::STATUS_REQUEST_CLI_EXTENSION
                   | checkhandshake::STATUS_REQUEST_SRV_EXTENSION
                   | checkhandshake::POST_HANDSHAKE_AUTH_CLI_EXTENSION,
                   "status_request handshake with client auth test");
}

#Test 7: A client auth handshake
$proxy->clear();
$proxy->clientflags("-enable_pha -cert ".srctop_file("apps", "server.pem"));
$proxy->serverflags("-Verify 5");
$proxy->start();
checkhandshake($proxy, checkhandshake::CLIENT_AUTH_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS |
               checkhandshake::POST_HANDSHAKE_AUTH_CLI_EXTENSION,
               "Client auth handshake test");

#Test 8: Server name handshake (no client request)
$proxy->clear();
$proxy->clientflags("-noservername");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               & ~checkhandshake::SERVER_NAME_CLI_EXTENSION,
               "Server name handshake test (client)");

#Test 9: Server name handshake (server support only)
$proxy->clear();
$proxy->clientflags("-noservername");
$proxy->serverflags("-servername testhost");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               & ~checkhandshake::SERVER_NAME_CLI_EXTENSION,
               "Server name handshake test (server)");

#Test 10: Server name handshake (client and server)
$proxy->clear();
$proxy->clientflags("-servername testhost");
$proxy->serverflags("-servername testhost");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::SERVER_NAME_SRV_EXTENSION,
               "Server name handshake test");

#Test 11: ALPN handshake (client request only)
$proxy->clear();
$proxy->clientflags("-alpn test");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::ALPN_CLI_EXTENSION,
               "ALPN handshake test (client)");

#Test 12: ALPN handshake (server support only)
$proxy->clear();
$proxy->serverflags("-alpn test");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS,
               "ALPN handshake test (server)");

#Test 13: ALPN handshake (client and server)
$proxy->clear();
$proxy->clientflags("-alpn test");
$proxy->serverflags("-alpn test");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::ALPN_CLI_EXTENSION
               | checkhandshake::ALPN_SRV_EXTENSION,
               "ALPN handshake test");

SKIP: {
    skip "No CT, EC or OCSP support in this OpenSSL build", 1
        if disabled("ct") || disabled("ec") || disabled("ocsp");

    #Test 14: SCT handshake (client request only)
    $proxy->clear();
    #Note: -ct also sends status_request
    $proxy->clientflags("-ct");
    $proxy->serverflags("-status_file "
                        .srctop_file("test", "recipes", "ocsp-response.der")
                        ." -serverinfo ".srctop_file("test", "serverinfo2.pem"));
    $proxy->start();
    checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS
                   | checkhandshake::SCT_CLI_EXTENSION
                   | checkhandshake::SCT_SRV_EXTENSION
                   | checkhandshake::STATUS_REQUEST_CLI_EXTENSION
                   | checkhandshake::STATUS_REQUEST_SRV_EXTENSION,
                   "SCT handshake test");
}

#Test 15: HRR Handshake
$proxy->clear();
$proxy->serverflags("-curves P-256");
$proxy->start();
checkhandshake($proxy, checkhandshake::HRR_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::KEY_SHARE_HRR_EXTENSION,
               "HRR handshake test");

#Test 16: Resumption handshake with HRR
$proxy->clear();
$proxy->clientflags("-sess_in ".$session);
$proxy->serverflags("-curves P-256");
$proxy->start();
checkhandshake($proxy, checkhandshake::HRR_RESUME_HANDSHAKE,
               (checkhandshake::DEFAULT_EXTENSIONS
                | checkhandshake::KEY_SHARE_HRR_EXTENSION
                | checkhandshake::PSK_CLI_EXTENSION
                | checkhandshake::PSK_SRV_EXTENSION),
               "Resumption handshake with HRR test");

#Test 17: Acceptable but non preferred key_share
$proxy->clear();
$proxy->clientflags("-curves P-256");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::SUPPORTED_GROUPS_SRV_EXTENSION,
               "Acceptable but non preferred key_share");

unlink $session;
