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

my $test_name = "test_sslcertstatus";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs the ocsp feature enabled"
    if disabled("ocsp");

plan skip_all => "$test_name needs TLS enabled"
    if alldisabled(available_protocols("tls"))
       || (!disabled("tls1_3") && disabled("tls1_2"));

my $proxy = TLSProxy::Proxy->new(
    \&certstatus_filter,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

#Test 1: Sending a status_request extension in both ClientHello and
#ServerHello but then omitting the CertificateStatus message is valid
$proxy->clientflags("-status -no_tls1_3");
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 1;
ok(TLSProxy::Message->success, "Missing CertificateStatus message");

sub certstatus_filter
{
    my $proxy = shift;

    # We're only interested in the initial ServerHello
    if ($proxy->flight != 1) {
        return;
    }

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_SERVER_HELLO) {
            #Add the status_request to the ServerHello even though we are not
            #going to send a CertificateStatus message
            $message->set_extension(TLSProxy::Message::EXT_STATUS_REQUEST,
                                    "");

            $message->repack();
        }
    }
}
