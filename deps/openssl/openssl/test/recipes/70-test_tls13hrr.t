#! /usr/bin/env perl
# Copyright 2017-2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;
use TLSProxy::Proxy;
use TLSProxy::Message;

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

my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

use constant {
    CHANGE_HRR_CIPHERSUITE => 0,
    CHANGE_CH1_CIPHERSUITE => 1,
    DUPLICATE_HRR => 2,
    INVALID_GROUP => 3,
    NO_SUPPORTED_VERSIONS => 4
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
plan tests => 5;
ok(TLSProxy::Message->fail(), "Server ciphersuite changes");

#Test 2: It is an error if the client changes the offered ciphersuites so that
#        we end up selecting a different ciphersuite between HRR and the SH
$proxy->clear();
if (disabled("ec")) {
    $proxy->serverflags("-curves ffdhe3072");
} else {
    $proxy->serverflags("-curves P-384");
}
$proxy->ciphersuitess("TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384");
$testtype = CHANGE_CH1_CIPHERSUITE;
$proxy->start();
ok(TLSProxy::Message->fail(), "Client ciphersuite changes");

#Test 3: A client should fail with unexpected_message alert if the server
#        sends more than 1 HRR
my $fatal_alert = 0;
$proxy->clear();
if (disabled("ec")) {
    $proxy->serverflags("-curves ffdhe3072");
} else {
    $proxy->serverflags("-curves P-384");
}
$testtype = DUPLICATE_HRR;
$proxy->start();
ok($fatal_alert, "Server duplicated HRR");

#Test 4: If the client sends a group that is in the supported_groups list but
#        otherwise not valid (e.g. not suitable for TLSv1.3) we should reject it
#        and not consider it when sending the HRR. We send brainpoolP512r1 in
#        the ClientHello, which is acceptable to the server but is not valid in
#        TLSv1.3. We expect the server to select P-521 in the HRR and the
#        handshake to complete successfully
SKIP: {
    skip "EC/TLSv1.2 is disabled in this build", 1
        if disabled("ec") || disabled("tls1_2");

    $proxy->clear();
    $proxy->clientflags("-groups P-256:brainpoolP512r1:P-521");
    $proxy->serverflags("-groups brainpoolP512r1:P-521");
    $testtype = INVALID_GROUP;
    $proxy->start();
    ok(TLSProxy::Message->success(), "Invalid group with HRR");
}

#Test 5: A failure should occur if an HRR is sent without the supported_versions
#        extension
$fatal_alert = 0;
$proxy->clear();
if (disabled("ec")) {
    $proxy->serverflags("-curves ffdhe3072");
} else {
    $proxy->serverflags("-curves P-384");
}
$testtype = NO_SUPPORTED_VERSIONS;
$proxy->start();
ok($fatal_alert, "supported_versions missing from HRR");

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

    if ($testtype == NO_SUPPORTED_VERSIONS) {
        # Check if we have the expected fatal alert
        if ($proxy->flight == 2) {
            $fatal_alert = 1
                if @{$proxy->record_list}[-1]->is_fatal_alert(0) == TLSProxy::Message::AL_DESC_MISSING_EXTENSION;
            return;
        }

        # Otherwise we're only interested in the HRR
        if ($proxy->flight != 1) {
            return;
        }

        my $hrr = ${$proxy->message_list}[1];
        $hrr->delete_extension(TLSProxy::Message::EXT_SUPPORTED_VERSIONS);
        $hrr->repack();
        return;
    }

    if ($testtype == DUPLICATE_HRR) {
        # We're only interested in the HRR
        # and the unexpected_message alert from client
        if ($proxy->flight == 4) {
            $fatal_alert = 1
                if @{$proxy->record_list}[-1]->is_fatal_alert(0) == TLSProxy::Message::AL_DESC_UNEXPECTED_MESSAGE;
            return;
        }
        if ($proxy->flight != 3) {
            return;
        }

        # Find ServerHello record (HRR actually) and insert after that
        my $i;
        for ($i = 0; ${$proxy->record_list}[$i]->flight() < 1; $i++) {
            next;
        }
        my $hrr_record = ${$proxy->record_list}[$i];
        my $dup_hrr = TLSProxy::Record->new(3,
            $hrr_record->content_type(),
            $hrr_record->version(),
            $hrr_record->len(),
            $hrr_record->sslv2(),
            $hrr_record->len_real(),
            $hrr_record->decrypt_len(),
            $hrr_record->data(),
            $hrr_record->decrypt_data());

        $i++;
        splice @{$proxy->record_list}, $i, 0, $dup_hrr;
        return;
    }

    if ($proxy->flight != 0) {
        return;
    }

    my $ch1 = ${$proxy->message_list}[0];

    if ($testtype == CHANGE_CH1_CIPHERSUITE) {
        # The server will always pick TLS_AES_256_GCM_SHA384
        my @ciphersuites = (TLSProxy::Message::CIPHER_TLS13_AES_128_GCM_SHA256);
        $ch1->ciphersuite_len(2 * scalar @ciphersuites);
        $ch1->ciphersuites(\@ciphersuites);
    } elsif ($testtype == INVALID_GROUP) {
        # INVALID_GROUP
        my $ext = pack "C7",
            0x00, 0x05, #List Length
            0x00, 0x1c, #brainpoolP512r1 (not compatible with TLSv1.3)
            0x00, 0x01, 0xff; #key_exchange data
        $ch1->set_extension(
            TLSProxy::Message::EXT_KEY_SHARE, $ext);
    }
    $ch1->repack();
}
