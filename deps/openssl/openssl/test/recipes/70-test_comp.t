#! /usr/bin/env perl
# Copyright 2017-2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
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

# The injected compression field.
use constant {
    MULTIPLE_COMPRESSIONS => 0, # Includes NULL, OK for >=TLS1.2
    NON_NULL_COMPRESSION => 1, # Alert for all TLS versions
    MULTIPLE_NO_NULL => 2, # Alert for all TLS versions
    NO_COMPRESSION => 3, # Alert for all TLS versions
    NULL_COMPRESSION => 4, # OK for all TLS versions
};
my %test_type_message = (
    MULTIPLE_COMPRESSIONS, "multiple, including null compression",
    NON_NULL_COMPRESSION, "one, not null compression",
    MULTIPLE_NO_NULL, "multiple, no null compression",
    NO_COMPRESSION, "no compression",
    NULL_COMPRESSION, "one, null compression",
);
my %compression_field_for_test = (
    # [null, unknown]
    MULTIPLE_COMPRESSIONS, [0x00, 0xff],
    # [unknown]
    NON_NULL_COMPRESSION, [0xff],
    # [unknown, unknown, unknown]
    MULTIPLE_NO_NULL, [0xfd, 0xfe, 0xff],
    # []
    NO_COMPRESSION, [],
    # [null]
    NULL_COMPRESSION, [0x00],
);
my $testtype;

# The tested TLS version
use constant {
    TEST_TLS_1_2 => 0, # Test TLSv1.2 and older
    TEST_TLS_1_3 => 1, # Test TLSv1.3 and newer
};
my %test_tls_message = (
    TEST_TLS_1_2, "TLS version 1.2 or older",
    TEST_TLS_1_3, "TLS version 1.3 or newer",
);

# The expected result from a test
use constant {
    EXPECT_SUCCESS => 0,
    EXPECT_DECODE_ERROR => 1,
    EXPECT_ILLEGAL_PARAMETER => 2,
};
my %expect_message = (
    EXPECT_SUCCESS, "Expected success",
    EXPECT_DECODE_ERROR, "Expected decode error",
    EXPECT_ILLEGAL_PARAMETER, "Expected illegal parameter alert",
);

my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 10;

SKIP: {
    skip "TLSv1.2 disabled", 5 if disabled("tls1_2");

    # Test 1: Check that sending multiple compression methods in a TLSv1.2
    #         ClientHello succeeds
    do_test(TEST_TLS_1_2, MULTIPLE_COMPRESSIONS, EXPECT_SUCCESS);

    # Test 2: Check that sending a non-null compression method in a TLSv1.2
    #         ClientHello results in an illegal parameter alert
    do_test(TEST_TLS_1_2, NON_NULL_COMPRESSION, EXPECT_ILLEGAL_PARAMETER);

    # Test 3: Check that sending multiple compression methods without null in
    #         a TLSv1.2 ClientHello results in an illegal parameter alert
    do_test(TEST_TLS_1_2, MULTIPLE_NO_NULL, EXPECT_ILLEGAL_PARAMETER);

    # Test 4: Check that sending no compression methods in a TLSv1.2
    #         ClientHello results in a decode error
    do_test(TEST_TLS_1_2, NO_COMPRESSION, EXPECT_DECODE_ERROR);

    # Test 5: Check that sending only null compression in a TLSv1.2
    #         ClientHello succeeds
    do_test(TEST_TLS_1_2, NULL_COMPRESSION, EXPECT_SUCCESS);
}

SKIP: {
    skip "TLSv1.3 disabled", 5
        if disabled("tls1_3") || (disabled("ec") && disabled("dh"));

    # Test 6: Check that sending multiple compression methods in a TLSv1.3
    #         ClientHello results in an illegal parameter alert
    do_test(TEST_TLS_1_3, MULTIPLE_COMPRESSIONS, EXPECT_ILLEGAL_PARAMETER);

    # Test 7: Check that sending a non-null compression method in a TLSv1.3
    #         ClientHello results in an illegal parameter alert
    do_test(TEST_TLS_1_3, NON_NULL_COMPRESSION, EXPECT_ILLEGAL_PARAMETER);

    # Test 8: Check that sending multiple compression methods without null in
    #         a TLSv1.3 ClientHello results in an illegal parameter alert
    do_test(TEST_TLS_1_3, MULTIPLE_NO_NULL, EXPECT_ILLEGAL_PARAMETER);

    # Test 9: Check that sending no compression methods in a TLSv1.3
    #         ClientHello results in a decode error
    do_test(TEST_TLS_1_3, NO_COMPRESSION, EXPECT_DECODE_ERROR);

    # Test 10: Check that sending only null compression in a TLSv1.3
    #          ClientHello succeeds
    do_test(TEST_TLS_1_3, NULL_COMPRESSION, EXPECT_SUCCESS);
}

sub do_test
{
    my $tls = shift; # The tested TLS version.
    my $type = shift; # The test type to perform.
    my $expect = shift; # The expected result.

    $proxy->clear();
    $proxy->filter(\&add_comp_filter);
    if ($tls == TEST_TLS_1_2) {
        $proxy->clientflags("-no_tls1_3");
    } else {
        $proxy->clientflags("-min_protocol TLSv1.3");
    }
    $testtype = $type;
    $proxy->start();
    print $expect, $tls, $type , "\n";
    my $failure_message = $expect_message{$expect} . " for " .
                          $test_tls_message{$tls} . " with " .
                          $test_type_message{$type};
    if ($expect == EXPECT_SUCCESS) {
        ok(TLSProxy::Message->success(), $failure_message);
    } elsif ($expect == EXPECT_DECODE_ERROR) {
        ok(is_alert_message(TLSProxy::Message::AL_DESC_DECODE_ERROR),
           $failure_message);
    } elsif ($expect == EXPECT_ILLEGAL_PARAMETER) {
        ok(is_alert_message(TLSProxy::Message::AL_DESC_ILLEGAL_PARAMETER),
           $failure_message);
    } else {
        die "Unexpected test expectation: $expect";
    }
}

# Test if the last message was a failure and matches the expected type.
sub is_alert_message
{
    my $alert_type = shift;
    return 0 unless TLSProxy::Message->fail();
    return 1 if TLSProxy::Message->alert->description() == $alert_type;
    return 0;
}

# Filter to insert the selected compression method into the hello message.
sub add_comp_filter
{
    my $proxy = shift;
    my $message;
    my @comp;

    # Only look at the ClientHello
    return if $proxy->flight != 0;

    $message = ${$proxy->message_list}[0];

    return if (!defined $message
               || $message->mt != TLSProxy::Message::MT_CLIENT_HELLO);

    @comp = @{$compression_field_for_test{$testtype}};
    $message->comp_meths(\@comp);
    $message->comp_meth_len(scalar @comp);
    $message->repack();
}
