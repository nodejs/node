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

my $test_name = "test_tls13hrr";
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

my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

use constant {
    CHANGE_HRR_CIPHERSUITE => 0,
    CHANGE_CH1_CIPHERSUITE => 1
};

#Test 1: A client should fail if the server changes the ciphersuite between the
#        HRR and the SH
$proxy->filter(\&hrr_filter);
if (disabled("ec")) {
    $proxy->serverflags("-curves ffdhe3072");
} else {
    $proxy->serverflags("-curves P-256");
}
my $testtype = CHANGE_HRR_CIPHERSUITE;
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 2;
ok(TLSProxy::Message->fail(), "Server ciphersuite changes");

#Test 2: It is an error if the client changes the offered ciphersuites so that
#        we end up selecting a different ciphersuite between HRR and the SH
$proxy->clear();
if (disabled("ec")) {
    $proxy->serverflags("-curves ffdhe3072");
} else {
    $proxy->serverflags("-curves P-256");
}
$proxy->ciphersuitess("TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384");
$testtype = CHANGE_CH1_CIPHERSUITE;
$proxy->start();
ok(TLSProxy::Message->fail(), "Client ciphersuite changes");

sub hrr_filter
{
    my $proxy = shift;

    if ($testtype == CHANGE_HRR_CIPHERSUITE) {
        # We're only interested in the HRR
        if ($proxy->flight != 1) {
            return;
        }

        my $hrr = ${$proxy->message_list}[1];

        # We will normally only ever select CIPHER_TLS13_AES_128_GCM_SHA256
        # because that's what Proxy tells s_server to do. Setting as below means
        # the ciphersuite will change will we get the ServerHello
        $hrr->ciphersuite(TLSProxy::Message::CIPHER_TLS13_AES_256_GCM_SHA384);
        $hrr->repack();
        return;
    }

    # CHANGE_CH1_CIPHERSUITE
    if ($proxy->flight != 0) {
        return;
    }

    my $ch1 = ${$proxy->message_list}[0];

    # The server will always pick TLS_AES_256_GCM_SHA384
    my @ciphersuites = (TLSProxy::Message::CIPHER_TLS13_AES_128_GCM_SHA256);
    $ch1->ciphersuite_len(2 * scalar @ciphersuites);
    $ch1->ciphersuites(\@ciphersuites);
    $ch1->repack();
}
