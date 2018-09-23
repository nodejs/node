#! /usr/bin/env perl
# Copyright 2016-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;
use TLSProxy::Proxy;

my $test_name = "test_sslrecords";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLSv1.2 enabled"
    if disabled("tls1_2");

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';
my $proxy = TLSProxy::Proxy->new(
    \&add_empty_recs_filter,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

#Test 1: Injecting out of context empty records should fail
my $content_type = TLSProxy::Record::RT_APPLICATION_DATA;
my $inject_recs_num = 1;
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
my $num_tests = 10;
if (!disabled("tls1_1")) {
    $num_tests++;
}
plan tests => $num_tests;
ok(TLSProxy::Message->fail(), "Out of context empty records test");

#Test 2: Injecting in context empty records should succeed
$proxy->clear();
$content_type = TLSProxy::Record::RT_HANDSHAKE;
$proxy->start();
ok(TLSProxy::Message->success(), "In context empty records test");

#Test 3: Injecting too many in context empty records should fail
$proxy->clear();
#We allow 32 consecutive in context empty records
$inject_recs_num = 33;
$proxy->start();
ok(TLSProxy::Message->fail(), "Too many in context empty records test");

#Test 4: Injecting a fragmented fatal alert should fail. We actually expect no
#        alerts to be sent from either side because *we* injected the fatal
#        alert, i.e. this will look like a disorderly close
$proxy->clear();
$proxy->filter(\&add_frag_alert_filter);
$proxy->start();
ok(!TLSProxy::Message->end(), "Fragmented alert records test");

#Run some SSLv2 ClientHello tests

use constant {
    TLSV1_2_IN_SSLV2 => 0,
    SSLV2_IN_SSLV2 => 1,
    FRAGMENTED_IN_TLSV1_2 => 2,
    FRAGMENTED_IN_SSLV2 => 3,
    ALERT_BEFORE_SSLV2 => 4
};
#Test 5: Inject an SSLv2 style record format for a TLSv1.2 ClientHello
my $sslv2testtype = TLSV1_2_IN_SSLV2;
$proxy->clear();
$proxy->filter(\&add_sslv2_filter);
$proxy->start();
ok(TLSProxy::Message->success(), "TLSv1.2 in SSLv2 ClientHello test");

#Test 6: Inject an SSLv2 style record format for an SSLv2 ClientHello. We don't
#        support this so it should fail. We actually treat it as an unknown
#        protocol so we don't even send an alert in this case.
$sslv2testtype = SSLV2_IN_SSLV2;
$proxy->clear();
$proxy->start();
ok(!TLSProxy::Message->end(), "SSLv2 in SSLv2 ClientHello test");

#Test 7: Sanity check ClientHello fragmentation. This isn't really an SSLv2 test
#        at all, but it gives us confidence that Test 8 fails for the right
#        reasons
$sslv2testtype = FRAGMENTED_IN_TLSV1_2;
$proxy->clear();
$proxy->start();
ok(TLSProxy::Message->success(), "Fragmented ClientHello in TLSv1.2 test");

#Test 8: Fragment a TLSv1.2 ClientHello across a TLS1.2 record; an SSLv2
#        record; and another TLS1.2 record. This isn't allowed so should fail
$sslv2testtype = FRAGMENTED_IN_SSLV2;
$proxy->clear();
$proxy->start();
ok(TLSProxy::Message->fail(), "Fragmented ClientHello in TLSv1.2/SSLv2 test");

#Test 9: Send a TLS warning alert before an SSLv2 ClientHello. This should
#        fail because an SSLv2 ClientHello must be the first record.
$sslv2testtype = ALERT_BEFORE_SSLV2;
$proxy->clear();
$proxy->start();
ok(TLSProxy::Message->fail(), "Alert before SSLv2 ClientHello test");

#Unrecognised record type tests

#Test 10: Sending an unrecognised record type in TLS1.2 should fail
$proxy->clear();
$proxy->filter(\&add_unknown_record_type);
$proxy->start();
ok(TLSProxy::Message->fail(), "Unrecognised record type in TLS1.2");

#Test 11: Sending an unrecognised record type in TLS1.1 should fail
if (!disabled("tls1_1")) {
    $proxy->clear();
    $proxy->clientflags("-tls1_1");
    $proxy->start();
    ok(TLSProxy::Message->fail(), "Unrecognised record type in TLS1.1");
}

sub add_empty_recs_filter
{
    my $proxy = shift;

    # We're only interested in the initial ClientHello
    if ($proxy->flight != 0) {
        return;
    }

    for (my $i = 0; $i < $inject_recs_num; $i++) {
        my $record = TLSProxy::Record->new(
            0,
            $content_type,
            TLSProxy::Record::VERS_TLS_1_2,
            0,
            0,
            0,
            0,
            "",
            ""
        );

        push @{$proxy->record_list}, $record;
    }
}

sub add_frag_alert_filter
{
    my $proxy = shift;
    my $byte;

    # We're only interested in the initial ClientHello
    if ($proxy->flight != 0) {
        return;
    }

    # Add a zero length fragment first
    #my $record = TLSProxy::Record->new(
    #    0,
    #    TLSProxy::Record::RT_ALERT,
    #    TLSProxy::Record::VERS_TLS_1_2,
    #    0,
    #    0,
    #    0,
    #    "",
    #    ""
    #);
    #push @{$proxy->record_list}, $record;

    # Now add the alert level (Fatal) as a separate record
    $byte = pack('C', TLSProxy::Message::AL_LEVEL_FATAL);
    my $record = TLSProxy::Record->new(
        0,
        TLSProxy::Record::RT_ALERT,
        TLSProxy::Record::VERS_TLS_1_2,
        1,
        0,
        1,
        1,
        $byte,
        $byte
    );
    push @{$proxy->record_list}, $record;

    # And finally the description (Unexpected message) in a third record
    $byte = pack('C', TLSProxy::Message::AL_DESC_UNEXPECTED_MESSAGE);
    $record = TLSProxy::Record->new(
        0,
        TLSProxy::Record::RT_ALERT,
        TLSProxy::Record::VERS_TLS_1_2,
        1,
        0,
        1,
        1,
        $byte,
        $byte
    );
    push @{$proxy->record_list}, $record;
}

sub add_sslv2_filter
{
    my $proxy = shift;
    my $clienthello;
    my $record;

    # We're only interested in the initial ClientHello
    if ($proxy->flight != 0) {
        return;
    }

    # Ditch the real ClientHello - we're going to replace it with our own
    shift @{$proxy->record_list};

    if ($sslv2testtype == ALERT_BEFORE_SSLV2) {
        my $alert = pack('CC', TLSProxy::Message::AL_LEVEL_FATAL,
                               TLSProxy::Message::AL_DESC_NO_RENEGOTIATION);
        my $alertlen = length $alert;
        $record = TLSProxy::Record->new(
            0,
            TLSProxy::Record::RT_ALERT,
            TLSProxy::Record::VERS_TLS_1_2,
            $alertlen,
            0,
            $alertlen,
            $alertlen,
            $alert,
            $alert
        );

        push @{$proxy->record_list}, $record;
    }

    if ($sslv2testtype == ALERT_BEFORE_SSLV2
            || $sslv2testtype == TLSV1_2_IN_SSLV2
            || $sslv2testtype == SSLV2_IN_SSLV2) {
        # This is an SSLv2 format ClientHello
        $clienthello =
            pack "C44",
            0x01, # ClientHello
            0x03, 0x03, #TLSv1.2
            0x00, 0x03, # Ciphersuites len
            0x00, 0x00, # Session id len
            0x00, 0x20, # Challenge len
            0x00, 0x00, 0x2f, #AES128-SHA
            0x01, 0x18, 0x9F, 0x76, 0xEC, 0x57, 0xCE, 0xE5, 0xB3, 0xAB, 0x79, 0x90,
            0xAD, 0xAC, 0x6E, 0xD1, 0x58, 0x35, 0x03, 0x97, 0x16, 0x10, 0x82, 0x56,
            0xD8, 0x55, 0xFF, 0xE1, 0x8A, 0xA3, 0x2E, 0xF6; # Challenge

        if ($sslv2testtype == SSLV2_IN_SSLV2) {
            # Set the version to "real" SSLv2
            vec($clienthello, 1, 8) = 0x00;
            vec($clienthello, 2, 8) = 0x02;
        }

        my $chlen = length $clienthello;

        $record = TLSProxy::Record->new(
            0,
            TLSProxy::Record::RT_HANDSHAKE,
            TLSProxy::Record::VERS_TLS_1_2,
            $chlen,
            1, #SSLv2
            $chlen,
            $chlen,
            $clienthello,
            $clienthello
        );

        push @{$proxy->record_list}, $record;
    } else {
        # For this test we're using a real TLS ClientHello
        $clienthello =
            pack "C49",
            0x01, # ClientHello
            0x00, 0x00, 0x2D, # Message length
            0x03, 0x03, # TLSv1.2
            0x01, 0x18, 0x9F, 0x76, 0xEC, 0x57, 0xCE, 0xE5, 0xB3, 0xAB, 0x79, 0x90,
            0xAD, 0xAC, 0x6E, 0xD1, 0x58, 0x35, 0x03, 0x97, 0x16, 0x10, 0x82, 0x56,
            0xD8, 0x55, 0xFF, 0xE1, 0x8A, 0xA3, 0x2E, 0xF6, # Random
            0x00, # Session id len
            0x00, 0x04, # Ciphersuites len
            0x00, 0x2f, # AES128-SHA
            0x00, 0xff, # Empty reneg info SCSV
            0x01, # Compression methods len
            0x00, # Null compression
            0x00, 0x00; # Extensions len

        # Split this into 3: A TLS record; a SSLv2 record and a TLS record.
        # We deliberately split the second record prior to the Challenge/Random
        # and set the first byte of the random to 1. This makes the second SSLv2
        # record look like an SSLv2 ClientHello
        my $frag1 = substr $clienthello, 0, 6;
        my $frag2 = substr $clienthello, 6, 32;
        my $frag3 = substr $clienthello, 38;

        my $fraglen = length $frag1;
        $record = TLSProxy::Record->new(
            0,
            TLSProxy::Record::RT_HANDSHAKE,
            TLSProxy::Record::VERS_TLS_1_2,
            $fraglen,
            0,
            $fraglen,
            $fraglen,
            $frag1,
            $frag1
        );
        push @{$proxy->record_list}, $record;

        $fraglen = length $frag2;
        my $recvers;
        if ($sslv2testtype == FRAGMENTED_IN_SSLV2) {
            $recvers = 1;
        } else {
            $recvers = 0;
        }
        $record = TLSProxy::Record->new(
            0,
            TLSProxy::Record::RT_HANDSHAKE,
            TLSProxy::Record::VERS_TLS_1_2,
            $fraglen,
            $recvers,
            $fraglen,
            $fraglen,
            $frag2,
            $frag2
        );
        push @{$proxy->record_list}, $record;

        $fraglen = length $frag3;
        $record = TLSProxy::Record->new(
            0,
            TLSProxy::Record::RT_HANDSHAKE,
            TLSProxy::Record::VERS_TLS_1_2,
            $fraglen,
            0,
            $fraglen,
            $fraglen,
            $frag3,
            $frag3
        );
        push @{$proxy->record_list}, $record;
    }

}

sub add_unknown_record_type
{
    my $proxy = shift;

    # We'll change a record after the initial version neg has taken place
    if ($proxy->flight != 2) {
        return;
    }

    my $lastrec = ${$proxy->record_list}[-1];
    my $record = TLSProxy::Record->new(
        2,
        TLSProxy::Record::RT_UNKNOWN,
        $lastrec->version(),
        1,
        0,
        1,
        1,
        "X",
        "X"
    );

    unshift @{$proxy->record_list}, $record;
}
