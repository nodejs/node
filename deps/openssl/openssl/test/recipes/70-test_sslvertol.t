#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;
use TLSProxy::Proxy;

my $test_name = "test_sslvertol";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLS enabled"
    if alldisabled(available_protocols("tls"));

my $proxy = TLSProxy::Proxy->new(
    \&vers_tolerance_filter,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

my @available_tls_versions = ();
foreach (available_protocols("tls")) {
    unless (disabled($_)) {
        note("Checking enabled protocol $_");
        m|^([a-z]+)(\d)(_\d)?|;
        my $versionname;
        if (defined $3) {
            $versionname = 'TLSProxy::Record::VERS_'.uc($1).'_'.$2.$3;
            note("'$1', '$2', '$3' => $versionname");
        } else {
            $versionname = 'TLSProxy::Record::VERS_'.uc($1).'_'.$2.'_0';
            note("'$1', '$2' => $versionname");
        }
        push @available_tls_versions, eval $versionname;
    }
}
note("TLS versions we can expect: ", join(", ", @available_tls_versions));

#This file does tests without the supported_versions extension.
#See 70-test_sslversions.t for tests with supported versions.

#Test 1: Asking for TLS1.4 should pass and negotiate the maximum
#available TLS version according to configuration below TLS1.3
my $client_version = TLSProxy::Record::VERS_TLS_1_4;
my $previous_version = tls_version_below(TLSProxy::Record::VERS_TLS_1_3);
$proxy->clientflags("-no_tls1_3");
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 3;
SKIP: {
    skip "There are too few protocols enabled for test 1", 1
        unless defined $previous_version;

    my $record = pop @{$proxy->record_list};
    ok((note("Record version received: ".$record->version()),
        TLSProxy::Message->success())
       && $record->version() == $previous_version,
       "Version tolerance test, below TLS 1.4 and not TLS 1.3");
}

#Test 2: Asking for TLS1.3 with that disabled should succeed and negotiate
#the highest configured TLS version below that.
$client_version = TLSProxy::Record::VERS_TLS_1_3;
$previous_version = tls_version_below($client_version);
SKIP: {
    skip "There are too few protocols enabled for test 2", 1
        unless defined $previous_version;

    $proxy->clear();
    $proxy->clientflags("-no_tls1_3");
    $proxy->start();
    my $record = pop @{$proxy->record_list};
    ok((note("Record version received: ".$record->version()),
        TLSProxy::Message->success())
       && $record->version() == $previous_version,
       "Version tolerance test, max version but not TLS 1.3");
}

#Test 3: Testing something below SSLv3 should fail.  We must disable TLS 1.3
#to avoid having the 'supported_versions' extension kick in and override our
#desires.
$client_version = TLSProxy::Record::VERS_SSL_3_0 - 1;
$proxy->clear();
$proxy->clientflags("-no_tls1_3");
$proxy->start();
my $record = pop @{$proxy->record_list};
ok((note("Record version received: ".
         (defined $record ? $record->version() : "none")),
    TLSProxy::Message->fail()),
   "Version tolerance test, SSL < 3.0");

sub vers_tolerance_filter
{
    my $proxy = shift;

    # We're only interested in the initial ClientHello
    if ($proxy->flight != 0) {
        return;
    }

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_CLIENT_HELLO) {
            #Set the client version
            #Anything above the max supported version should succeed
            #Anything below SSLv3 should fail
            $message->client_version($client_version);
            $message->repack();
        }
    }
}

sub tls_version_below {
    if (@_) {
        my $term = shift;
        my $res = undef;

        foreach (@available_tls_versions) {
            $res = $_ if $_ < $term;
        }
        return $res;
    }
    return $available_tls_versions[-1];
}
