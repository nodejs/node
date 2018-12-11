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
use File::Temp qw(tempfile);

use constant {
    REVERSE_ORDER_VERSIONS => 1,
    UNRECOGNISED_VERSIONS => 2,
    NO_EXTENSION => 3,
    EMPTY_EXTENSION => 4,
    TLS1_1_AND_1_0_ONLY => 5,
    WITH_TLS1_4 => 6,
    BAD_LEGACY_VERSION => 7
};

my $testtype;

my $test_name = "test_sslversions";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLS1.3, TLS1.2 and TLS1.1 enabled"
    if disabled("tls1_3") || disabled("tls1_2") || disabled("tls1_1");

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';

my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

#We're just testing various negative and unusual scenarios here. ssltest with
#02-protocol-version.conf should check all the various combinations of normal
#version neg

#Test 1: An empty supported_versions extension should not succeed
$testtype = EMPTY_EXTENSION;
$proxy->filter(\&modify_supported_versions_filter);
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 8;
ok(TLSProxy::Message->fail(), "Empty supported versions");

#Test 2: supported_versions extension with no recognised versions should not
#succeed
$proxy->clear();
$testtype = UNRECOGNISED_VERSIONS;
$proxy->start();
ok(TLSProxy::Message->fail(), "No recognised versions");

#Test 3: No supported versions extensions should succeed and select TLSv1.2
$proxy->clear();
$testtype = NO_EXTENSION;
$proxy->start();
my $record = pop @{$proxy->record_list};
ok(TLSProxy::Message->success()
   && $record->version() == TLSProxy::Record::VERS_TLS_1_2,
   "No supported versions extension");

#Test 4: No supported versions extensions should fail if only TLS1.3 available
$proxy->clear();
$proxy->serverflags("-tls1_3");
$proxy->start();
ok(TLSProxy::Message->fail(), "No supported versions extension (only TLS1.3)");

#Test 5: supported versions extension with best version last should succeed
#and select TLSv1.3
$proxy->clear();
$testtype = REVERSE_ORDER_VERSIONS;
$proxy->start();
$record = pop @{$proxy->record_list};
ok(TLSProxy::Message->success()
   && $record->version() == TLSProxy::Record::VERS_TLS_1_2
   && TLSProxy::Proxy->is_tls13(),
   "Reverse order versions");

#Test 6: no TLSv1.3 or TLSv1.2 version in supported versions extension, but
#TLSv1.1 and TLSv1.0 are present. Should just use TLSv1.1 and succeed
$proxy->clear();
$testtype = TLS1_1_AND_1_0_ONLY;
$proxy->start();
$record = pop @{$proxy->record_list};
ok(TLSProxy::Message->success()
   && $record->version() == TLSProxy::Record::VERS_TLS_1_1,
   "TLS1.1 and TLS1.0 in supported versions extension only");

#Test 7: TLS1.4 and TLS1.3 in supported versions. Should succeed and use TLS1.3
$proxy->clear();
$testtype = WITH_TLS1_4;
$proxy->start();
$record = pop @{$proxy->record_list};
ok(TLSProxy::Message->success()
   && $record->version() == TLSProxy::Record::VERS_TLS_1_2
   && TLSProxy::Proxy->is_tls13(),
   "TLS1.4 in supported versions extension");

#Test 8: Set the legacy version to SSLv3 with supported versions. Should fail
$proxy->clear();
$testtype = BAD_LEGACY_VERSION;
$proxy->start();
ok(TLSProxy::Message->fail(), "Legacy version is SSLv3 with supported versions");

sub modify_supported_versions_filter
{
    my $proxy = shift;

    if ($proxy->flight == 1) {
        # Change the ServerRandom so that the downgrade sentinel doesn't cause
        # the connection to fail
        my $message = ${$proxy->message_list}[1];
        return if (!defined $message);

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
            my $ext;
            if ($testtype == REVERSE_ORDER_VERSIONS) {
                $ext = pack "C5",
                    0x04, # Length
                    0x03, 0x03, #TLSv1.2
                    0x03, 0x04; #TLSv1.3
            } elsif ($testtype == UNRECOGNISED_VERSIONS) {
                $ext = pack "C5",
                    0x04, # Length
                    0x04, 0x04, #Some unrecognised version
                    0x04, 0x03; #Another unrecognised version
            } elsif ($testtype == TLS1_1_AND_1_0_ONLY) {
                $ext = pack "C5",
                    0x04, # Length
                    0x03, 0x02, #TLSv1.1
                    0x03, 0x01; #TLSv1.0
            } elsif ($testtype == WITH_TLS1_4) {
                    $ext = pack "C5",
                        0x04, # Length
                        0x03, 0x05, #TLSv1.4
                        0x03, 0x04; #TLSv1.3
            }
            if ($testtype == REVERSE_ORDER_VERSIONS
                    || $testtype == UNRECOGNISED_VERSIONS
                    || $testtype == TLS1_1_AND_1_0_ONLY
                    || $testtype == WITH_TLS1_4) {
                $message->set_extension(
                    TLSProxy::Message::EXT_SUPPORTED_VERSIONS, $ext);
            } elsif ($testtype == EMPTY_EXTENSION) {
                $message->set_extension(
                    TLSProxy::Message::EXT_SUPPORTED_VERSIONS, "");
            } elsif ($testtype == NO_EXTENSION) {
                $message->delete_extension(
                    TLSProxy::Message::EXT_SUPPORTED_VERSIONS);
            } else {
                # BAD_LEGACY_VERSION
                $message->client_version(TLSProxy::Record::VERS_SSL_3_0);
            }

            $message->repack();
        }
    }
}
