#! /usr/bin/env perl
# Copyright 2015-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;
use TLSProxy::Proxy;
use File::Temp qw(tempfile);

use constant {
    LOOK_ONLY => 0,
    EMPTY_EXTENSION => 1,
    MISSING_EXTENSION => 2,
    NO_ACCEPTABLE_KEY_SHARES => 3,
    NON_PREFERRED_KEY_SHARE => 4,
    ACCEPTABLE_AT_END => 5,
    NOT_IN_SUPPORTED_GROUPS => 6,
    GROUP_ID_TOO_SHORT => 7,
    KEX_LEN_MISMATCH => 8,
    ZERO_LEN_KEX_DATA => 9,
    TRAILING_DATA => 10,
    SELECT_X25519 => 11,
    NO_KEY_SHARES_IN_HRR => 12,
    NON_TLS1_3_KEY_SHARE => 13
};

use constant {
    CLIENT_TO_SERVER => 1,
    SERVER_TO_CLIENT => 2
};


use constant {
    X25519 => 0x1d,
    P_256 => 0x17,
    FFDHE2048 => 0x0100,
    FFDHE3072 => 0x0101
};

my $testtype;
my $direction;
my $selectedgroupid;

my $test_name = "test_key_share";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLS1.3 enabled"
    if disabled("tls1_3");

plan skip_all => "$test_name needs EC or DH enabled"
    if disabled("ec") && disabled("dh");

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';

my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

#We assume that test_ssl_new and friends will test the happy path for this,
#so we concentrate on the less common scenarios

#Test 1: An empty key_shares extension should succeed after a HelloRetryRequest
$testtype = EMPTY_EXTENSION;
$direction = CLIENT_TO_SERVER;
$proxy->filter(\&modify_key_shares_filter);
if (disabled("ec")) {
    $proxy->serverflags("-groups ffdhe3072");
} else {
    $proxy->serverflags("-groups P-256");
}
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 23;
ok(TLSProxy::Message->success(), "Success after HRR");

#Test 2: The server sending an HRR requesting a group the client already sent
#        should fail
$proxy->clear();
$proxy->start();
ok(TLSProxy::Message->fail(), "Server asks for group already provided");

#Test 3: A missing key_shares extension should not succeed
$proxy->clear();
$testtype = MISSING_EXTENSION;
$proxy->start();
ok(TLSProxy::Message->fail(), "Missing key_shares extension");

#Test 4: No initial acceptable key_shares should succeed after a
#        HelloRetryRequest
$proxy->clear();
$proxy->filter(undef);
if (disabled("ec")) {
    $proxy->serverflags("-groups ffdhe3072");
} else {
    $proxy->serverflags("-groups P-256");
}
$proxy->start();
ok(TLSProxy::Message->success(), "No initial acceptable key_shares");

#Test 5: No acceptable key_shares and no shared groups should fail
$proxy->clear();
$proxy->filter(undef);
if (disabled("ec")) {
    $proxy->serverflags("-groups ffdhe2048");
} else {
    $proxy->serverflags("-groups P-256");
}
if (disabled("ec")) {
    $proxy->clientflags("-groups ffdhe3072");
} else {
    $proxy->clientflags("-groups P-384");
}
$proxy->start();
ok(TLSProxy::Message->fail(), "No acceptable key_shares");

#Test 6: A non preferred but acceptable key_share should succeed
$proxy->clear();
$proxy->clientflags("-curves P-256");
if (disabled("ec")) {
    $proxy->clientflags("-groups ffdhe3072");
} else {
    $proxy->clientflags("-groups P-256");
}
$proxy->start();
ok(TLSProxy::Message->success(), "Non preferred key_share");
$proxy->filter(\&modify_key_shares_filter);

SKIP: {
    skip "No ec support in this OpenSSL build", 1 if disabled("ec");

    #Test 7: An acceptable key_share after a list of non-acceptable ones should
    #succeed
    $proxy->clear();
    $testtype = ACCEPTABLE_AT_END;
    $proxy->start();
    ok(TLSProxy::Message->success(), "Acceptable key_share at end of list");
}

#Test 8: An acceptable key_share but for a group not in supported_groups should
#fail
$proxy->clear();
$testtype = NOT_IN_SUPPORTED_GROUPS;
$proxy->start();
ok(TLSProxy::Message->fail(), "Acceptable key_share not in supported_groups");

#Test 9: Too short group_id should fail
$proxy->clear();
$testtype = GROUP_ID_TOO_SHORT;
$proxy->start();
ok(TLSProxy::Message->fail(), "Group id too short");

#Test 10: key_exchange length mismatch should fail
$proxy->clear();
$testtype = KEX_LEN_MISMATCH;
$proxy->start();
ok(TLSProxy::Message->fail(), "key_exchange length mismatch");

#Test 11: Zero length key_exchange should fail
$proxy->clear();
$testtype = ZERO_LEN_KEX_DATA;
$proxy->start();
ok(TLSProxy::Message->fail(), "zero length key_exchange data");

#Test 12: Trailing data on key_share list should fail
$proxy->clear();
$testtype = TRAILING_DATA;
$proxy->start();
ok(TLSProxy::Message->fail(), "key_share list trailing data");

#Test 13: Multiple acceptable key_shares - we choose the first one
$proxy->clear();
$direction = SERVER_TO_CLIENT;
$testtype = LOOK_ONLY;
$selectedgroupid = 0;
if (disabled("ec")) {
    $proxy->clientflags("-groups ffdhe3072:ffdhe2048");
} else {
    $proxy->clientflags("-groups P-256:X25519");
}
$proxy->start();
if (disabled("ec")) {
    ok(TLSProxy::Message->success() && ($selectedgroupid == FFDHE3072),
       "Multiple acceptable key_shares");
} else {
    ok(TLSProxy::Message->success() && ($selectedgroupid == P_256),
       "Multiple acceptable key_shares");
}

#Test 14: Multiple acceptable key_shares - we choose the first one (part 2)
$proxy->clear();
if (disabled("ec")) {
    $proxy->clientflags("-curves ffdhe2048:ffdhe3072");
} else {
    $proxy->clientflags("-curves X25519:P-256");
}
$proxy->start();
if (disabled("ec")) {
    ok(TLSProxy::Message->success() && ($selectedgroupid == FFDHE2048),
       "Multiple acceptable key_shares (part 2)");
} else {
    ok(TLSProxy::Message->success() && ($selectedgroupid == X25519),
       "Multiple acceptable key_shares (part 2)");
}

#Test 15: Server sends key_share that wasn't offered should fail
$proxy->clear();
$testtype = SELECT_X25519;
if (disabled("ec")) {
    $proxy->clientflags("-groups ffdhe3072");
} else {
    $proxy->clientflags("-groups P-256");
}
$proxy->start();
ok(TLSProxy::Message->fail(), "Non offered key_share");

#Test 16: Too short group_id in ServerHello should fail
$proxy->clear();
$testtype = GROUP_ID_TOO_SHORT;
$proxy->start();
ok(TLSProxy::Message->fail(), "Group id too short in ServerHello");

#Test 17: key_exchange length mismatch in ServerHello should fail
$proxy->clear();
$testtype = KEX_LEN_MISMATCH;
$proxy->start();
ok(TLSProxy::Message->fail(), "key_exchange length mismatch in ServerHello");

#Test 18: Zero length key_exchange in ServerHello should fail
$proxy->clear();
$testtype = ZERO_LEN_KEX_DATA;
$proxy->start();
ok(TLSProxy::Message->fail(), "zero length key_exchange data in ServerHello");

#Test 19: Trailing data on key_share in ServerHello should fail
$proxy->clear();
$testtype = TRAILING_DATA;
$proxy->start();
ok(TLSProxy::Message->fail(), "key_share trailing data in ServerHello");

SKIP: {
    skip "No TLSv1.2 support in this OpenSSL build", 2 if disabled("tls1_2");

    #Test 20: key_share should not be sent if the client is not capable of
    #         negotiating TLSv1.3
    $proxy->clear();
    $proxy->filter(undef);
    $proxy->clientflags("-no_tls1_3");
    $proxy->start();
    my $clienthello = $proxy->message_list->[0];
    ok(TLSProxy::Message->success()
       && !defined $clienthello->extension_data->{TLSProxy::Message::EXT_KEY_SHARE},
       "No key_share for TLS<=1.2 client");
    $proxy->filter(\&modify_key_shares_filter);

    #Test 21: A server not capable of negotiating TLSv1.3 should not attempt to
    #         process a key_share
    $proxy->clear();
    $direction = CLIENT_TO_SERVER;
    $testtype = NO_ACCEPTABLE_KEY_SHARES;
    $proxy->serverflags("-no_tls1_3");
    $proxy->start();
    ok(TLSProxy::Message->success(), "Ignore key_share for TLS<=1.2 server");
}

#Test 22: The server sending an HRR but not requesting a new key_share should
#         fail
$proxy->clear();
$direction = SERVER_TO_CLIENT;
$testtype = NO_KEY_SHARES_IN_HRR;
if (disabled("ec")) {
    $proxy->serverflags("-groups ffdhe2048");
} else {
    $proxy->serverflags("-groups X25519");
}
$proxy->start();
ok(TLSProxy::Message->fail(), "Server sends HRR with no key_shares");

SKIP: {
    skip "No EC support in this OpenSSL build", 1 if disabled("ec");
    #Test 23: Trailing data on key_share in ServerHello should fail
    $proxy->clear();
    $direction = CLIENT_TO_SERVER;
    $proxy->clientflags("-groups secp192r1:P-256:X25519");
    $proxy->ciphers("AES128-SHA:\@SECLEVEL=0");
    $testtype = NON_TLS1_3_KEY_SHARE;
    $proxy->start();
    my $ishrr = defined ${$proxy->message_list}[2]
                &&(${$proxy->message_list}[0]->mt == TLSProxy::Message::MT_CLIENT_HELLO)
                && (${$proxy->message_list}[2]->mt == TLSProxy::Message::MT_CLIENT_HELLO);
    ok(TLSProxy::Message->success() && $ishrr,
       "Client sends a key_share for a Non TLSv1.3 group");
}

sub modify_key_shares_filter
{
    my $proxy = shift;

    # We're only interested in the initial ClientHello/SererHello/HRR
    if (($direction == CLIENT_TO_SERVER && $proxy->flight != 0
                && ($proxy->flight != 1 || $testtype != NO_KEY_SHARES_IN_HRR))
            || ($direction == SERVER_TO_CLIENT && $proxy->flight != 1)) {
        return;
    }

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_CLIENT_HELLO
                && $direction == CLIENT_TO_SERVER) {
            my $ext;
            my $suppgroups;

            if ($testtype != NON_TLS1_3_KEY_SHARE) {
                #Setup supported groups to include some unrecognised groups
                $suppgroups = pack "C8",
                    0x00, 0x06, #List Length
                    0xff, 0xfe, #Non existing group 1
                    0xff, 0xff, #Non existing group 2
                    0x00, 0x1d; #x25519
            } else {
                $suppgroups = pack "C6",
                    0x00, 0x04, #List Length
                    0x00, 0x13,
                    0x00, 0x1d; #x25519
            }

            if ($testtype == EMPTY_EXTENSION) {
                $ext = pack "C2",
                    0x00, 0x00;
            } elsif ($testtype == NO_ACCEPTABLE_KEY_SHARES) {
                $ext = pack "C12",
                    0x00, 0x0a, #List Length
                    0xff, 0xfe, #Non existing group 1
                    0x00, 0x01, 0xff, #key_exchange data
                    0xff, 0xff, #Non existing group 2
                    0x00, 0x01, 0xff; #key_exchange data
            } elsif ($testtype == ACCEPTABLE_AT_END) {
                $ext = pack "C11H64",
                    0x00, 0x29, #List Length
                    0xff, 0xfe, #Non existing group 1
                    0x00, 0x01, 0xff, #key_exchange data
                    0x00, 0x1d, #x25519
                    0x00, 0x20, #key_exchange data length
                    "155155B95269ED5C87EAA99C2EF5A593".
                    "EDF83495E80380089F831B94D14B1421";  #key_exchange data
            } elsif ($testtype == NOT_IN_SUPPORTED_GROUPS) {
                $suppgroups = pack "C4",
                    0x00, 0x02, #List Length
                    0x00, 0xfe; #Non existing group 1
            } elsif ($testtype == GROUP_ID_TOO_SHORT) {
                $ext = pack "C6H64C1",
                    0x00, 0x25, #List Length
                    0x00, 0x1d, #x25519
                    0x00, 0x20, #key_exchange data length
                    "155155B95269ED5C87EAA99C2EF5A593".
                    "EDF83495E80380089F831B94D14B1421";  #key_exchange data
                    0x00;       #Group id too short
            } elsif ($testtype == KEX_LEN_MISMATCH) {
                $ext = pack "C8",
                    0x00, 0x06, #List Length
                    0x00, 0x1d, #x25519
                    0x00, 0x20, #key_exchange data length
                    0x15, 0x51; #Only two bytes of data, but length should be 32
            } elsif ($testtype == ZERO_LEN_KEX_DATA) {
                $ext = pack "C10H64",
                    0x00, 0x28, #List Length
                    0xff, 0xfe, #Non existing group 1
                    0x00, 0x00, #zero length key_exchange data is invalid
                    0x00, 0x1d, #x25519
                    0x00, 0x20, #key_exchange data length
                    "155155B95269ED5C87EAA99C2EF5A593".
                    "EDF83495E80380089F831B94D14B1421";  #key_exchange data
            } elsif ($testtype == TRAILING_DATA) {
                $ext = pack "C6H64C1",
                    0x00, 0x24, #List Length
                    0x00, 0x1d, #x25519
                    0x00, 0x20, #key_exchange data length
                    "155155B95269ED5C87EAA99C2EF5A593".
                    "EDF83495E80380089F831B94D14B1421", #key_exchange data
                    0x00; #Trailing garbage
            } elsif ($testtype == NO_KEY_SHARES_IN_HRR) {
                #We trick the server into thinking we sent a P-256 key_share -
                #but the client actually sent X25519
                $ext = pack "C7",
                    0x00, 0x05, #List Length
                    0x00, 0x17, #P-256
                    0x00, 0x01, #key_exchange data length
                    0xff;       #Dummy key_share data
            } elsif ($testtype == NON_TLS1_3_KEY_SHARE) {
                $ext = pack "C6H98",
                    0x00, 0x35, #List Length
                    0x00, 0x13, #P-192
                    0x00, 0x31, #key_exchange data length
                    "04EE3B38D1CB800A1A2B702FC8423599F2AC7161E175C865F8".
                    "3DAF78BCBAE561464E8144359BE70CB7989D28A2F43F8F2C";  #key_exchange data
            }

            if ($testtype != EMPTY_EXTENSION
                    && $testtype != NO_KEY_SHARES_IN_HRR) {
                $message->set_extension(
                    TLSProxy::Message::EXT_SUPPORTED_GROUPS, $suppgroups);
            }
            if ($testtype == MISSING_EXTENSION) {
                $message->delete_extension(
                    TLSProxy::Message::EXT_KEY_SHARE);
            } elsif ($testtype != NOT_IN_SUPPORTED_GROUPS) {
                $message->set_extension(
                    TLSProxy::Message::EXT_KEY_SHARE, $ext);
            }

            $message->repack();
        } elsif ($message->mt == TLSProxy::Message::MT_SERVER_HELLO
                     && $direction == SERVER_TO_CLIENT) {
            my $ext;
            my $key_share =
                $message->extension_data->{TLSProxy::Message::EXT_KEY_SHARE};
            $selectedgroupid = unpack("n", $key_share);

            if ($testtype == LOOK_ONLY) {
                return;
            }
            if ($testtype == NO_KEY_SHARES_IN_HRR) {
                $message->delete_extension(TLSProxy::Message::EXT_KEY_SHARE);
                $message->set_extension(TLSProxy::Message::EXT_UNKNOWN, "");
                $message->repack();
                return;
            }
            if ($testtype == SELECT_X25519) {
                $ext = pack "C4H64",
                    0x00, 0x1d, #x25519
                    0x00, 0x20, #key_exchange data length
                    "155155B95269ED5C87EAA99C2EF5A593".
                    "EDF83495E80380089F831B94D14B1421";  #key_exchange data
            } elsif ($testtype == GROUP_ID_TOO_SHORT) {
                $ext = pack "C1",
                    0x00;
            } elsif ($testtype == KEX_LEN_MISMATCH) {
                $ext = pack "C6",
                    0x00, 0x1d, #x25519
                    0x00, 0x20, #key_exchange data length
                    0x15, 0x51; #Only two bytes of data, but length should be 32
            } elsif ($testtype == ZERO_LEN_KEX_DATA) {
                $ext = pack "C4",
                    0x00, 0x1d, #x25519
                    0x00, 0x00, #zero length key_exchange data is invalid
            } elsif ($testtype == TRAILING_DATA) {
                $ext = pack "C4H64C1",
                    0x00, 0x1d, #x25519
                    0x00, 0x20, #key_exchange data length
                    "155155B95269ED5C87EAA99C2EF5A593".
                    "EDF83495E80380089F831B94D14B1421", #key_exchange data
                    0x00; #Trailing garbage
            }
            $message->set_extension(TLSProxy::Message::EXT_KEY_SHARE, $ext);

            $message->repack();
        }
    }
}


