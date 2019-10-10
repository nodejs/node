#! /usr/bin/env perl
# Copyright 2015-2019 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file srctop_dir bldtop_dir/;
use OpenSSL::Test::Utils;
use File::Temp qw(tempfile);
use TLSProxy::Proxy;
use checkhandshake qw(checkhandshake @handmessages @extensions);

my $test_name = "test_sslmessages";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLS enabled"
    if alldisabled(available_protocols("tls"))
       || (!disabled("tls1_3") && disabled("tls1_2"));

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';
$ENV{CTLOG_FILE} = srctop_file("test", "ct", "log_list.conf");

my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

@handmessages = (
    [TLSProxy::Message::MT_CLIENT_HELLO,
        checkhandshake::ALL_HANDSHAKES],
    [TLSProxy::Message::MT_SERVER_HELLO,
        checkhandshake::ALL_HANDSHAKES],
    [TLSProxy::Message::MT_CERTIFICATE,
        checkhandshake::ALL_HANDSHAKES
        & ~checkhandshake::RESUME_HANDSHAKE],
    (disabled("ec") ? () :
                      [TLSProxy::Message::MT_SERVER_KEY_EXCHANGE,
                          checkhandshake::EC_HANDSHAKE]),
    [TLSProxy::Message::MT_CERTIFICATE_STATUS,
        checkhandshake::OCSP_HANDSHAKE],
    #ServerKeyExchange handshakes not currently supported by TLSProxy
    [TLSProxy::Message::MT_CERTIFICATE_REQUEST,
        checkhandshake::CLIENT_AUTH_HANDSHAKE],
    [TLSProxy::Message::MT_SERVER_HELLO_DONE,
        checkhandshake::ALL_HANDSHAKES
        & ~checkhandshake::RESUME_HANDSHAKE],
    [TLSProxy::Message::MT_CERTIFICATE,
        checkhandshake::CLIENT_AUTH_HANDSHAKE],
    [TLSProxy::Message::MT_CLIENT_KEY_EXCHANGE,
        checkhandshake::ALL_HANDSHAKES
        & ~checkhandshake::RESUME_HANDSHAKE],
    [TLSProxy::Message::MT_CERTIFICATE_VERIFY,
        checkhandshake::CLIENT_AUTH_HANDSHAKE],
    [TLSProxy::Message::MT_NEXT_PROTO,
        checkhandshake::NPN_HANDSHAKE],
    [TLSProxy::Message::MT_FINISHED,
        checkhandshake::ALL_HANDSHAKES],
    [TLSProxy::Message::MT_NEW_SESSION_TICKET,
        checkhandshake::ALL_HANDSHAKES
        & ~checkhandshake::RESUME_HANDSHAKE],
    [TLSProxy::Message::MT_FINISHED,
        checkhandshake::ALL_HANDSHAKES],
    [TLSProxy::Message::MT_CLIENT_HELLO,
        checkhandshake::RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_SERVER_HELLO,
        checkhandshake::RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_CERTIFICATE,
        checkhandshake::RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_SERVER_HELLO_DONE,
        checkhandshake::RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_CLIENT_KEY_EXCHANGE,
        checkhandshake::RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_FINISHED,
        checkhandshake::RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_NEW_SESSION_TICKET,
        checkhandshake::RENEG_HANDSHAKE],
    [TLSProxy::Message::MT_FINISHED,
        checkhandshake::RENEG_HANDSHAKE],
    [0, 0]
);

@extensions = (
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SERVER_NAME,
        TLSProxy::Message::CLIENT,
        checkhandshake::SERVER_NAME_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_STATUS_REQUEST,
        TLSProxy::Message::CLIENT,
        checkhandshake::STATUS_REQUEST_CLI_EXTENSION],
    (disabled("ec") ? () :
                      [TLSProxy::Message::MT_CLIENT_HELLO,
                       TLSProxy::Message::EXT_SUPPORTED_GROUPS,
                       TLSProxy::Message::CLIENT,
                       checkhandshake::DEFAULT_EXTENSIONS]),
    (disabled("ec") ? () :
                      [TLSProxy::Message::MT_CLIENT_HELLO,
                       TLSProxy::Message::EXT_EC_POINT_FORMATS,
                       TLSProxy::Message::CLIENT,
                       checkhandshake::DEFAULT_EXTENSIONS]),
    (disabled("tls1_2") ? () :
     [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SIG_ALGS,
        TLSProxy::Message::CLIENT,
         checkhandshake::DEFAULT_EXTENSIONS]),
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
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_RENEGOTIATE,
        TLSProxy::Message::CLIENT,
        checkhandshake::RENEGOTIATE_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_NPN,
        TLSProxy::Message::CLIENT,
        checkhandshake::NPN_CLI_EXTENSION],
    [TLSProxy::Message::MT_CLIENT_HELLO, TLSProxy::Message::EXT_SRP,
        TLSProxy::Message::CLIENT,
        checkhandshake::SRP_CLI_EXTENSION],

    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_RENEGOTIATE,
        TLSProxy::Message::SERVER,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_ENCRYPT_THEN_MAC,
        TLSProxy::Message::SERVER,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_EXTENDED_MASTER_SECRET,
        TLSProxy::Message::SERVER,
        checkhandshake::DEFAULT_EXTENSIONS],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_SESSION_TICKET,
        TLSProxy::Message::SERVER,
        checkhandshake::SESSION_TICKET_SRV_EXTENSION],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_SERVER_NAME,
        TLSProxy::Message::SERVER,
        checkhandshake::SERVER_NAME_SRV_EXTENSION],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_STATUS_REQUEST,
        TLSProxy::Message::SERVER,
        checkhandshake::STATUS_REQUEST_SRV_EXTENSION],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_ALPN,
        TLSProxy::Message::SERVER,
        checkhandshake::ALPN_SRV_EXTENSION],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_SCT,
        TLSProxy::Message::SERVER,
        checkhandshake::SCT_SRV_EXTENSION],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_NPN,
        TLSProxy::Message::SERVER,
        checkhandshake::NPN_SRV_EXTENSION],
    [TLSProxy::Message::MT_SERVER_HELLO, TLSProxy::Message::EXT_EC_POINT_FORMATS,
        TLSProxy::Message::SERVER,
        checkhandshake::EC_POINT_FORMAT_SRV_EXTENSION],
    [0,0,0,0]
);

#Test 1: Check we get all the right messages for a default handshake
(undef, my $session) = tempfile();
$proxy->serverconnects(2);
$proxy->clientflags("-no_tls1_3 -sess_out ".$session);
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 21;
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS,
               "Default handshake test");

#Test 2: Resumption handshake
$proxy->clearClient();
$proxy->clientflags("-no_tls1_3 -sess_in ".$session);
$proxy->clientstart();
checkhandshake($proxy, checkhandshake::RESUME_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               & ~checkhandshake::SESSION_TICKET_SRV_EXTENSION,
               "Resumption handshake test");
unlink $session;

SKIP: {
    skip "No OCSP support in this OpenSSL build", 3
        if disabled("ocsp");

    #Test 3: A status_request handshake (client request only)
    $proxy->clear();
    $proxy->clientflags("-no_tls1_3 -status");
    $proxy->start();
    checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS
                   | checkhandshake::STATUS_REQUEST_CLI_EXTENSION,
                   "status_request handshake test (client)");

    #Test 4: A status_request handshake (server support only)
    $proxy->clear();
    $proxy->clientflags("-no_tls1_3");
    $proxy->serverflags("-status_file "
                        .srctop_file("test", "recipes", "ocsp-response.der"));
    $proxy->start();
    checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS,
                   "status_request handshake test (server)");

    #Test 5: A status_request handshake (client and server)
    $proxy->clear();
    $proxy->clientflags("-no_tls1_3 -status");
    $proxy->serverflags("-status_file "
                        .srctop_file("test", "recipes", "ocsp-response.der"));
    $proxy->start();
    checkhandshake($proxy, checkhandshake::OCSP_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS
                   | checkhandshake::STATUS_REQUEST_CLI_EXTENSION
                   | checkhandshake::STATUS_REQUEST_SRV_EXTENSION,
                   "status_request handshake test");
}

#Test 6: A client auth handshake
$proxy->clear();
$proxy->clientflags("-no_tls1_3 -cert ".srctop_file("apps", "server.pem"));
$proxy->serverflags("-Verify 5");
$proxy->start();
checkhandshake($proxy, checkhandshake::CLIENT_AUTH_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS,
               "Client auth handshake test");

#Test 7: A handshake with a renegotiation
$proxy->clear();
$proxy->clientflags("-no_tls1_3");
$proxy->reneg(1);
$proxy->start();
checkhandshake($proxy, checkhandshake::RENEG_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS,
               "Renegotiation handshake test");

#Test 8: Server name handshake (no client request)
$proxy->clear();
$proxy->clientflags("-no_tls1_3 -noservername");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               & ~checkhandshake::SERVER_NAME_CLI_EXTENSION,
               "Server name handshake test (client)");

#Test 9: Server name handshake (server support only)
$proxy->clear();
$proxy->clientflags("-no_tls1_3 -noservername");
$proxy->serverflags("-servername testhost");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               & ~checkhandshake::SERVER_NAME_CLI_EXTENSION,
               "Server name handshake test (server)");

#Test 10: Server name handshake (client and server)
$proxy->clear();
$proxy->clientflags("-no_tls1_3 -servername testhost");
$proxy->serverflags("-servername testhost");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::SERVER_NAME_SRV_EXTENSION,
               "Server name handshake test");

#Test 11: ALPN handshake (client request only)
$proxy->clear();
$proxy->clientflags("-no_tls1_3 -alpn test");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS
               | checkhandshake::ALPN_CLI_EXTENSION,
               "ALPN handshake test (client)");

#Test 12: ALPN handshake (server support only)
$proxy->clear();
$proxy->clientflags("-no_tls1_3");
$proxy->serverflags("-alpn test");
$proxy->start();
checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
               checkhandshake::DEFAULT_EXTENSIONS,
               "ALPN handshake test (server)");

#Test 13: ALPN handshake (client and server)
$proxy->clear();
$proxy->clientflags("-no_tls1_3 -alpn test");
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
    $proxy->clientflags("-no_tls1_3 -ct");
    $proxy->serverflags("-status_file "
                        .srctop_file("test", "recipes", "ocsp-response.der"));
    $proxy->start();
    checkhandshake($proxy, checkhandshake::OCSP_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS
                   | checkhandshake::SCT_CLI_EXTENSION
                   | checkhandshake::STATUS_REQUEST_CLI_EXTENSION
                   | checkhandshake::STATUS_REQUEST_SRV_EXTENSION,
                   "SCT handshake test (client)");
}

SKIP: {
    skip "No OCSP support in this OpenSSL build", 1
        if disabled("ocsp");

    #Test 15: SCT handshake (server support only)
    $proxy->clear();
    #Note: -ct also sends status_request
    $proxy->clientflags("-no_tls1_3");
    $proxy->serverflags("-status_file "
                        .srctop_file("test", "recipes", "ocsp-response.der"));
    $proxy->start();
    checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS,
                   "SCT handshake test (server)");
}

SKIP: {
    skip "No CT, EC or OCSP support in this OpenSSL build", 1
        if disabled("ct") || disabled("ec") || disabled("ocsp");

    #Test 16: SCT handshake (client and server)
    #There is no built-in server side support for this so we are actually also
    #testing custom extensions here
    $proxy->clear();
    #Note: -ct also sends status_request
    $proxy->clientflags("-no_tls1_3 -ct");
    $proxy->serverflags("-status_file "
                        .srctop_file("test", "recipes", "ocsp-response.der")
                        ." -serverinfo ".srctop_file("test", "serverinfo.pem"));
    $proxy->start();
    checkhandshake($proxy, checkhandshake::OCSP_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS
                   | checkhandshake::SCT_CLI_EXTENSION
                   | checkhandshake::SCT_SRV_EXTENSION
                   | checkhandshake::STATUS_REQUEST_CLI_EXTENSION
                   | checkhandshake::STATUS_REQUEST_SRV_EXTENSION,
                   "SCT handshake test");
}


SKIP: {
    skip "No NPN support in this OpenSSL build", 3
        if disabled("nextprotoneg");

    #Test 17: NPN handshake (client request only)
    $proxy->clear();
    $proxy->clientflags("-no_tls1_3 -nextprotoneg test");
    $proxy->start();
    checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS
                   | checkhandshake::NPN_CLI_EXTENSION,
                   "NPN handshake test (client)");

    #Test 18: NPN handshake (server support only)
    $proxy->clear();
    $proxy->clientflags("-no_tls1_3");
    $proxy->serverflags("-nextprotoneg test");
    $proxy->start();
    checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS,
                   "NPN handshake test (server)");

    #Test 19: NPN handshake (client and server)
    $proxy->clear();
    $proxy->clientflags("-no_tls1_3 -nextprotoneg test");
    $proxy->serverflags("-nextprotoneg test");
    $proxy->start();
    checkhandshake($proxy, checkhandshake::NPN_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS
                   | checkhandshake::NPN_CLI_EXTENSION
                   | checkhandshake::NPN_SRV_EXTENSION,
                   "NPN handshake test");
}

SKIP: {
    skip "No SRP support in this OpenSSL build", 1
        if disabled("srp");

    #Test 20: SRP extension
    #Note: We are not actually going to perform an SRP handshake (TLSProxy
    #does not support it). However it is sufficient for us to check that the
    #SRP extension gets added on the client side. There is no SRP extension
    #generated on the server side anyway.
    $proxy->clear();
    $proxy->clientflags("-no_tls1_3 -srpuser user -srppass pass:pass");
    $proxy->start();
    checkhandshake($proxy, checkhandshake::DEFAULT_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS
                   | checkhandshake::SRP_CLI_EXTENSION,
                   "SRP extension test");
}

#Test 21: EC handshake
SKIP: {
    skip "No EC support in this OpenSSL build", 1 if disabled("ec");
    $proxy->clear();
    $proxy->clientflags("-no_tls1_3");
    $proxy->serverflags("-no_tls1_3");
    $proxy->ciphers("ECDHE-RSA-AES128-SHA");
    $proxy->start();
    checkhandshake($proxy, checkhandshake::EC_HANDSHAKE,
                   checkhandshake::DEFAULT_EXTENSIONS
                   | checkhandshake::EC_POINT_FORMAT_SRV_EXTENSION,
                   "EC handshake test");
}
