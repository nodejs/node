#! /usr/bin/env perl
# Copyright 2017-2018 The OpenSSL Project Authors. All Rights Reserved.
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

my $test_name = "test_comp";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLSv1.3 or TLSv1.2 enabled"
    if disabled("tls1_3") && disabled("tls1_2");

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';
$ENV{CTLOG_FILE} = srctop_file("test", "ct", "log_list.conf");

use constant {
    MULTIPLE_COMPRESSIONS => 0,
    NON_NULL_COMPRESSION => 1
};
my $testtype;

my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 4;

SKIP: {
    skip "TLSv1.2 disabled", 2 if disabled("tls1_2");
    #Test 1: Check that sending multiple compression methods in a TLSv1.2
    #        ClientHello succeeds
    $proxy->clear();
    $proxy->filter(\&add_comp_filter);
    $proxy->clientflags("-no_tls1_3");
    $testtype = MULTIPLE_COMPRESSIONS;
    $proxy->start();
    ok(TLSProxy::Message->success(), "Non null compression");

    #Test 2: NULL compression method must be present in TLSv1.2
    $proxy->clear();
    $proxy->clientflags("-no_tls1_3");
    $testtype = NON_NULL_COMPRESSION;
    $proxy->start();
    ok(TLSProxy::Message->fail(), "NULL compression missing");
}

SKIP: {
    skip "TLSv1.3 disabled", 2 if disabled("tls1_3");
    #Test 3: Check that sending multiple compression methods in a TLSv1.3
    #        ClientHello fails
    $proxy->clear();
    $proxy->filter(\&add_comp_filter);
    $testtype = MULTIPLE_COMPRESSIONS;
    $proxy->start();
    ok(TLSProxy::Message->fail(), "Non null compression (TLSv1.3)");

    #Test 4: NULL compression method must be present in TLSv1.3
    $proxy->clear();
    $testtype = NON_NULL_COMPRESSION;
    $proxy->start();
    ok(TLSProxy::Message->fail(), "NULL compression missing (TLSv1.3)");
}

sub add_comp_filter
{
    my $proxy = shift;
    my $flight;
    my $message;
    my @comp;

    # Only look at the ClientHello
    return if $proxy->flight != 0;

    $message = ${$proxy->message_list}[0];

    return if (!defined $message
               || $message->mt != TLSProxy::Message::MT_CLIENT_HELLO);

    if ($testtype == MULTIPLE_COMPRESSIONS) {
        @comp = (
            0x00, #Null compression method
            0xff); #Unknown compression
    } elsif ($testtype == NON_NULL_COMPRESSION) {
        @comp = (0xff); #Unknown compression
    }
    $message->comp_meths(\@comp);
    $message->comp_meth_len(scalar @comp);
    $message->repack();
}
