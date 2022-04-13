#! /usr/bin/env perl
# Copyright 2017-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;
use TLSProxy::Proxy;

my $test_name = "test_tls13cookie";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLS1.3 enabled"
    if disabled("tls1_3") || (disabled("ec") && disabled("dh"));

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';

use constant {
    COOKIE_ONLY => 0,
    COOKIE_AND_KEY_SHARE => 1
};

my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

my $cookieseen = 0;
my $testtype;

#Test 1: Inserting a cookie into an HRR should see it echoed in the ClientHello
$testtype = COOKIE_ONLY;
$proxy->filter(\&cookie_filter);
$proxy->serverflags("-curves X25519") if !disabled("ec");
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 2;
SKIP: {
    skip "EC disabled", 1, if disabled("ec");
    ok(TLSProxy::Message->success() && $cookieseen == 1, "Cookie seen");
}



#Test 2: Same as test 1 but should also work where a new key_share is also
#        required
$testtype = COOKIE_AND_KEY_SHARE;
$proxy->clear();
if (disabled("ec")) {
    $proxy->clientflags("-curves ffdhe3072:ffdhe2048");
    $proxy->serverflags("-curves ffdhe2048");
} else {
    $proxy->clientflags("-curves P-256:X25519");
    $proxy->serverflags("-curves X25519");
}
$proxy->start();
ok(TLSProxy::Message->success() && $cookieseen == 1, "Cookie seen");

sub cookie_filter
{
    my $proxy = shift;

    # We're only interested in the HRR and both ClientHellos
    return if ($proxy->flight > 2);

    my $ext = pack "C8",
        0x00, 0x06, #Cookie Length
        0x00, 0x01, #Dummy cookie data (6 bytes)
        0x02, 0x03,
        0x04, 0x05;

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_SERVER_HELLO
                && ${$message->records}[0]->flight == 1) {
            $message->delete_extension(TLSProxy::Message::EXT_KEY_SHARE)
                if ($testtype == COOKIE_ONLY);
            $message->set_extension(TLSProxy::Message::EXT_COOKIE, $ext);
            $message->repack();
        } elsif ($message->mt == TLSProxy::Message::MT_CLIENT_HELLO) {
            if (${$message->records}[0]->flight == 0) {
                if ($testtype == COOKIE_ONLY) {
                    my $ext = pack "C7",
                        0x00, 0x05, #List Length
                        0x00, 0x17, #P-256
                        0x00, 0x01, #key_exchange data length
                        0xff;       #Dummy key_share data
                    # Trick the server into thinking we got an unacceptable
                    # key_share
                    $message->set_extension(
                        TLSProxy::Message::EXT_KEY_SHARE, $ext);
                    $message->repack();
                }
            } else {
                #cmp can behave differently dependent on locale
                no locale;
                my $cookie =
                    $message->extension_data->{TLSProxy::Message::EXT_COOKIE};

                return if !defined($cookie);

                return if ($cookie cmp $ext) != 0;

                $cookieseen = 1;
            }
        }
    }
}
