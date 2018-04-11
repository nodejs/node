#! /usr/bin/env perl
# Copyright 2015-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
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

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';
my $proxy = TLSProxy::Proxy->new(
    \&extension_filter,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

# Test 1: Sending a zero length extension block should pass
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 3;
ok(TLSProxy::Message->success, "Zero extension length test");

sub extension_filter
{
    my $proxy = shift;

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

# Test 2-3: Sending a duplicate extension should fail.
sub inject_duplicate_extension
{
  my ($proxy, $message_type) = @_;

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == $message_type) {
          my %extensions = %{$message->extension_data};
            # Add a duplicate (unknown) extension.
            $message->set_extension(TLSProxy::Message::EXT_DUPLICATE_EXTENSION, "");
            $message->set_extension(TLSProxy::Message::EXT_DUPLICATE_EXTENSION, "");
            $message->repack();
        }
    }
}

sub inject_duplicate_extension_clienthello
{
    my $proxy = shift;

    # We're only interested in the initial ClientHello
    if ($proxy->flight != 0) {
        return;
    }

    inject_duplicate_extension($proxy, TLSProxy::Message::MT_CLIENT_HELLO);
}

sub inject_duplicate_extension_serverhello
{
    my $proxy = shift;

    # We're only interested in the initial ServerHello
    if ($proxy->flight != 1) {
        return;
    }

    inject_duplicate_extension($proxy, TLSProxy::Message::MT_SERVER_HELLO);
}

$proxy->clear();
$proxy->filter(\&inject_duplicate_extension_clienthello);
$proxy->start();
ok(TLSProxy::Message->fail(), "Duplicate ClientHello extension");

$proxy->clear();
$proxy->filter(\&inject_duplicate_extension_serverhello);
$proxy->start();
ok(TLSProxy::Message->fail(), "Duplicate ServerHello extension");
