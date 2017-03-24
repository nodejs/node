#! /usr/bin/env perl
# Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;
use TLSProxy::Proxy;

my $test_name = "test_sslcbcpadding";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS|MSWin32)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLSv1.2 enabled"
    if disabled("tls1_2");

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';
my $proxy = TLSProxy::Proxy->new(
    \&add_maximal_padding_filter,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

# TODO: We could test all 256 values, but then the log file gets too large for
# CI. See https://github.com/openssl/openssl/issues/1440.
my @test_offsets = (0, 128, 254, 255);

# Test that maximally-padded records are accepted.
my $bad_padding_offset = -1;
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 1 + scalar(@test_offsets);
ok(TLSProxy::Message->success(), "Maximally-padded record test");

# Test that invalid padding is rejected.
foreach my $offset (@test_offsets) {
    $proxy->clear();
    $bad_padding_offset = $offset;
    $proxy->start();
    ok(TLSProxy::Message->fail(), "Invalid padding byte $bad_padding_offset");
}

sub add_maximal_padding_filter
{
    my $proxy = shift;

    if ($proxy->flight == 0) {
        # Disable Encrypt-then-MAC.
        foreach my $message (@{$proxy->message_list}) {
            if ($message->mt != TLSProxy::Message::MT_CLIENT_HELLO) {
                next;
            }

            $message->delete_extension(TLSProxy::Message::EXT_ENCRYPT_THEN_MAC);
            $message->process_extensions();
            $message->repack();
        }
    }

    if ($proxy->flight == 3) {
        # Insert a maximally-padded record. Assume a block size of 16 (AES) and
        # a MAC length of 20 (SHA-1).
        my $block_size = 16;
        my $mac_len = 20;

        # Size the plaintext so that 256 is a valid padding.
        my $plaintext_len = $block_size - ($mac_len % $block_size);
        my $plaintext = "A" x $plaintext_len;

        my $data = "B" x $block_size; # Explicit IV.
        $data .= $plaintext;
        $data .= TLSProxy::Proxy::fill_known_data($mac_len); # MAC.

        # Add padding.
        for (my $i = 0; $i < 256; $i++) {
            if ($i == $bad_padding_offset) {
                $data .= "\xfe";
            } else {
                $data .= "\xff";
            }
        }

        my $record = TLSProxy::Record->new(
            $proxy->flight,
            TLSProxy::Record::RT_APPLICATION_DATA,
            TLSProxy::Record::VERS_TLS_1_2,
            length($data),
            0,
            length($data),
            $plaintext_len,
            $data,
            $plaintext,
        );

        # Send the record immediately after the server Finished.
        push @{$proxy->record_list}, $record;
    }
}
