#! /usr/bin/env perl
# Copyright 2018-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;
use TLSProxy::Proxy;

my $test_name = "test_tls13alerts";
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

#Test 1: We test that a server can handle an unencrypted alert when normally the
#        next message is encrypted
$proxy->filter(\&alert_filter);
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 1;
my $alert = TLSProxy::Message->alert();
ok(TLSProxy::Message->fail() && !$alert->server() && !$alert->encrypted(), "Client sends an unecrypted alert");

sub alert_filter
{
    my $proxy = shift;

    if ($proxy->flight != 1) {
        return;
    }

    ${$proxy->message_list}[1]->session_id_len(1);
    ${$proxy->message_list}[1]->repack();
}
