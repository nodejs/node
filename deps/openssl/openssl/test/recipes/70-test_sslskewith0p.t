#! /usr/bin/env perl
# Copyright 2015-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;
use TLSProxy::Proxy;

my $test_name = "test_sslskewith0p";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "dh is not supported by this OpenSSL build"
    if disabled("dh");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLS enabled"
    if alldisabled(available_protocols("tls"));

my $proxy = TLSProxy::Proxy->new(
    \&ske_0_p_filter,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

#We must use an anon DHE cipher for this test
$proxy->cipherc('ADH-AES128-SHA:@SECLEVEL=0');
$proxy->ciphers('ADH-AES128-SHA:@SECLEVEL=0');

$proxy->clientflags("-no_tls1_3");
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 1;
ok(TLSProxy::Message->fail, "ServerKeyExchange with 0 p");

sub ske_0_p_filter
{
    my $proxy = shift;

    # We're only interested in the SKE - always in flight 1
    if ($proxy->flight != 1) {
        return;
    }

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_SERVER_KEY_EXCHANGE) {
            #Set p to a value of 0
            $message->p(pack('C', 0));

            $message->repack();
        }
    }
}
