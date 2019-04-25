#! /usr/bin/env perl
# Copyright 2015-2019 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use feature 'state';

use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;
use TLSProxy::Proxy;

my $test_name = "test_sslextension";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLS enabled"
    if alldisabled(available_protocols("tls"));

my $no_below_tls13 = alldisabled(("tls1", "tls1_1", "tls1_2"))
                     || (!disabled("tls1_3") && disabled("tls1_2"));

use constant {
    UNSOLICITED_SERVER_NAME => 0,
    UNSOLICITED_SERVER_NAME_TLS13 => 1,
    UNSOLICITED_SCT => 2,
    NONCOMPLIANT_SUPPORTED_GROUPS => 3
};

my $testtype;
my $fatal_alert = 0;    # set by filter on fatal alert

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';
my $proxy = TLSProxy::Proxy->new(
    \&inject_duplicate_extension_clienthello,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);


sub extension_filter
{
    my $proxy = shift;

    if ($proxy->flight == 1) {
        # Change the ServerRandom so that the downgrade sentinel doesn't cause
        # the connection to fail
        my $message = ${$proxy->message_list}[1];
        $message->random("\0"x32);
        $message->repack();
        return;
    }

    # We're only interested in the initial ClientHello
    if ($proxy->flight != 0) {
        return;
    }

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_CLIENT_HELLO) {
            # Remove all extensions and set the extension len to zero
            $message->extension_data({});
            $message->extensions_len(0);
            # Extensions have been removed so make sure we don't try to use them
            $message->process_extensions();

            $message->repack();
        }
    }
}

sub inject_duplicate_extension
{
  my ($proxy, $message_type) = @_;

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == $message_type) {
          my %extensions = %{$message->extension_data};
            # Add a duplicate extension. We use cryptopro_bug since we never
            # normally write that one, and it is allowed as unsolicited in the
            # ServerHello
            $message->set_extension(TLSProxy::Message::EXT_CRYPTOPRO_BUG_EXTENSION, "");
            $message->dupext(TLSProxy::Message::EXT_CRYPTOPRO_BUG_EXTENSION);
            $message->repack();
        }
    }
}

sub inject_duplicate_extension_clienthello
{
    my $proxy = shift;

    # We're only interested in the initial ClientHello
    if ($proxy->flight == 0) {
        inject_duplicate_extension($proxy, TLSProxy::Message::MT_CLIENT_HELLO);
        return;
    }

    my $last_record = @{$proxy->{record_list}}[-1];
    $fatal_alert = 1 if $last_record->is_fatal_alert(1);
}

sub inject_duplicate_extension_serverhello
{
    my $proxy = shift;

    # We're only interested in the initial ServerHello
    if ($proxy->flight == 0) {
        return;
    } elsif ($proxy->flight == 1) {
        inject_duplicate_extension($proxy, TLSProxy::Message::MT_SERVER_HELLO);
        return;
    }

    my $last_record = @{$proxy->{record_list}}[-1];
    $fatal_alert = 1 if $last_record->is_fatal_alert(0);
}

sub inject_unsolicited_extension
{
    my $proxy = shift;
    my $message;
    state $sent_unsolisited_extension;

    if ($proxy->flight == 0) {
        $sent_unsolisited_extension = 0;
        return;
    }

    # We're only interested in the initial ServerHello/EncryptedExtensions
    if ($proxy->flight != 1) {
        if ($sent_unsolisited_extension) {
            my $last_record = @{$proxy->record_list}[-1];
            $fatal_alert = 1 if $last_record->is_fatal_alert(0);
        }
        return;
    }

    if ($testtype == UNSOLICITED_SERVER_NAME_TLS13) {
        return if (!defined($message = ${$proxy->message_list}[2]));
        die "Expecting EE message ".($message->mt).","
                                   .${$proxy->message_list}[1]->mt.", "
                                   .${$proxy->message_list}[3]->mt
            if $message->mt != TLSProxy::Message::MT_ENCRYPTED_EXTENSIONS;
    } else {
        $message = ${$proxy->message_list}[1];
    }

    my $ext = pack "C2",
        0x00, 0x00; #Extension length

    my $type;
    if ($testtype == UNSOLICITED_SERVER_NAME
            || $testtype == UNSOLICITED_SERVER_NAME_TLS13) {
        $type = TLSProxy::Message::EXT_SERVER_NAME;
    } elsif ($testtype == UNSOLICITED_SCT) {
        $type = TLSProxy::Message::EXT_SCT;
    } elsif ($testtype == NONCOMPLIANT_SUPPORTED_GROUPS) {
        $type = TLSProxy::Message::EXT_SUPPORTED_GROUPS;
    }
    $message->set_extension($type, $ext);
    $message->repack();
    $sent_unsolisited_extension = 1;
}

sub inject_cryptopro_extension
{
    my $proxy = shift;

    # We're only interested in the initial ClientHello
    if ($proxy->flight != 0) {
        return;
    }

    my $message = ${$proxy->message_list}[0];
    $message->set_extension(TLSProxy::Message::EXT_CRYPTOPRO_BUG_EXTENSION, "");
    $message->repack();
}

# Test 1-2: Sending a duplicate extension should fail.
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 8;
ok($fatal_alert, "Duplicate ClientHello extension");

$fatal_alert = 0;
$proxy->clear();
$proxy->filter(\&inject_duplicate_extension_serverhello);
$proxy->start();
ok($fatal_alert, "Duplicate ServerHello extension");

SKIP: {
    skip "TLS <= 1.2 disabled", 3 if $no_below_tls13;

    #Test 3: Sending a zero length extension block should pass
    $proxy->clear();
    $proxy->filter(\&extension_filter);
    $proxy->start();
    ok(TLSProxy::Message->success, "Zero extension length test");

    #Test 4: Inject an unsolicited extension (<= TLSv1.2)
    $fatal_alert = 0;
    $proxy->clear();
    $proxy->filter(\&inject_unsolicited_extension);
    $testtype = UNSOLICITED_SERVER_NAME;
    $proxy->clientflags("-no_tls1_3 -noservername");
    $proxy->start();
    ok($fatal_alert, "Unsolicited server name extension");

    #Test 5: Inject a noncompliant supported_groups extension (<= TLSv1.2)
    $proxy->clear();
    $proxy->filter(\&inject_unsolicited_extension);
    $testtype = NONCOMPLIANT_SUPPORTED_GROUPS;
    $proxy->clientflags("-no_tls1_3");
    $proxy->start();
    ok(TLSProxy::Message->success(), "Noncompliant supported_groups extension");
}

SKIP: {
    skip "TLS <= 1.2 or CT disabled", 1
        if $no_below_tls13 || disabled("ct");
    #Test 6: Same as above for the SCT extension which has special handling
    $fatal_alert = 0;
    $proxy->clear();
    $testtype = UNSOLICITED_SCT;
    $proxy->clientflags("-no_tls1_3");
    $proxy->start();
    ok($fatal_alert, "Unsolicited sct extension");
}

SKIP: {
    skip "TLS 1.3 disabled", 1 if disabled("tls1_3");
    #Test 7: Inject an unsolicited extension (TLSv1.3)
    $fatal_alert = 0;
    $proxy->clear();
    $proxy->filter(\&inject_unsolicited_extension);
    $testtype = UNSOLICITED_SERVER_NAME_TLS13;
    $proxy->clientflags("-noservername");
    $proxy->start();
    ok($fatal_alert, "Unsolicited server name extension (TLSv1.3)");
}

#Test 8: Send the cryptopro extension in a ClientHello. Normally this is an
#        unsolicited extension only ever seen in the ServerHello. We should
#        ignore it in a ClientHello
$proxy->clear();
$proxy->filter(\&inject_cryptopro_extension);
$proxy->start();
ok(TLSProxy::Message->success(), "Cryptopro extension in ClientHello");
