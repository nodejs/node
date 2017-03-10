#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
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

my $test_name = "test_tlsextms";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS|MSWin32)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs TLS enabled"
    if alldisabled(available_protocols("tls"));

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';

sub checkmessages($$$$$);
sub setrmextms($$);
sub clearall();

my $crmextms = 0;
my $srmextms = 0;
my $cextms = 0;
my $sextms = 0;
my $fullhand = 0;

my $proxy = TLSProxy::Proxy->new(
    \&extms_filter,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

#Test 1: By default server and client should send extended master secret
# extension.
#Expected result: ClientHello extension seen; ServerHello extension seen
#                 Full handshake

setrmextms(0, 0);
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 9;
checkmessages(1, "Default extended master secret test", 1, 1, 1);

#Test 2: If client omits extended master secret extension, server should too.
#Expected result: ClientHello extension not seen; ServerHello extension not seen
#                 Full handshake

clearall();
setrmextms(1, 0);
$proxy->start();
checkmessages(2, "No client extension extended master secret test", 0, 0, 1);

# Test 3: same as 1 but with session tickets disabled.
# Expected result: same as test 1.

clearall();
$proxy->clientflags("-no_ticket");
setrmextms(0, 0);
$proxy->start();
checkmessages(3, "No ticket extended master secret test", 1, 1, 1);

# Test 4: same as 2 but with session tickets disabled.
# Expected result: same as test 2.

clearall();
$proxy->clientflags("-no_ticket");
setrmextms(1, 0);
$proxy->start();
checkmessages(2, "No ticket, no client extension extended master secret test", 0, 0, 1);

#Test 5: Session resumption extended master secret test
#
#Expected result: ClientHello extension seen; ServerHello extension seen
#                 Abbreviated handshake

clearall();
setrmextms(0, 0);
(undef, my $session) = tempfile();
$proxy->serverconnects(2);
$proxy->clientflags("-sess_out ".$session);
$proxy->start();
$proxy->clearClient();
$proxy->clientflags("-sess_in ".$session);
$proxy->clientstart();
checkmessages(5, "Session resumption extended master secret test", 1, 1, 0);
unlink $session;

#Test 6: Session resumption extended master secret test original session
# omits extension. Server must not resume session.
#Expected result: ClientHello extension seen; ServerHello extension seen
#                 Full handshake

clearall();
setrmextms(1, 0);
(undef, $session) = tempfile();
$proxy->serverconnects(2);
$proxy->clientflags("-sess_out ".$session);
$proxy->start();
$proxy->clearClient();
$proxy->clientflags("-sess_in ".$session);
setrmextms(0, 0);
$proxy->clientstart();
checkmessages(6, "Session resumption extended master secret test", 1, 1, 1);
unlink $session;

#Test 7: Session resumption extended master secret test resumed session
# omits client extension. Server must abort connection.
#Expected result: aborted connection.

clearall();
setrmextms(0, 0);
(undef, $session) = tempfile();
$proxy->serverconnects(2);
$proxy->clientflags("-sess_out ".$session);
$proxy->start();
$proxy->clearClient();
$proxy->clientflags("-sess_in ".$session);
setrmextms(1, 0);
$proxy->clientstart();
ok(TLSProxy::Message->fail(), "Client inconsistent session resumption");
unlink $session;

#Test 8: Session resumption extended master secret test resumed session
# omits server extension. Client must abort connection.
#Expected result: aborted connection.

clearall();
setrmextms(0, 0);
(undef, $session) = tempfile();
$proxy->serverconnects(2);
$proxy->clientflags("-sess_out ".$session);
$proxy->start();
$proxy->clearClient();
$proxy->clientflags("-sess_in ".$session);
setrmextms(0, 1);
$proxy->clientstart();
ok(TLSProxy::Message->fail(), "Server inconsistent session resumption 1");
unlink $session;

#Test 9: Session resumption extended master secret test initial session
# omits server extension. Client must abort connection.
#Expected result: aborted connection.

clearall();
setrmextms(0, 1);
(undef, $session) = tempfile();
$proxy->serverconnects(2);
$proxy->clientflags("-sess_out ".$session);
$proxy->start();
$proxy->clearClient();
$proxy->clientflags("-sess_in ".$session);
setrmextms(0, 0);
$proxy->clientstart();
ok(TLSProxy::Message->fail(), "Server inconsistent session resumption 2");
unlink $session;

sub extms_filter
{
    my $proxy = shift;

    foreach my $message (@{$proxy->message_list}) {
        if ($crmextms && $message->mt == TLSProxy::Message::MT_CLIENT_HELLO) {
            $message->delete_extension(TLSProxy::Message::EXT_EXTENDED_MASTER_SECRET);
            $message->repack();
        }
        if ($srmextms && $message->mt == TLSProxy::Message::MT_SERVER_HELLO) {
            $message->delete_extension(TLSProxy::Message::EXT_EXTENDED_MASTER_SECRET);
            $message->repack();
        }
    }
}

sub checkmessages($$$$$)
{
    my ($testno, $testname, $testcextms, $testsextms, $testhand) = @_;

    subtest $testname => sub {

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_CLIENT_HELLO
            || $message->mt == TLSProxy::Message::MT_SERVER_HELLO) {
        #Get the extensions data
        my %extensions = %{$message->extension_data};
        if (defined
            $extensions{TLSProxy::Message::EXT_EXTENDED_MASTER_SECRET}) {
            if ($message->mt == TLSProxy::Message::MT_CLIENT_HELLO) {
                $cextms = 1;
            } else {
                $sextms = 1;
            }
        }
        } elsif ($message->mt == TLSProxy::Message::MT_CLIENT_KEY_EXCHANGE) {
            #Must be doing a full handshake
            $fullhand = 1;
        }
    }

    plan tests => 4;

    ok(TLSProxy::Message->success, "Handshake");

    ok($testcextms == $cextms,
       "ClientHello extension extended master secret check");
    ok($testsextms == $sextms,
       "ServerHello extension extended master secret check");
    ok($testhand == $fullhand,
       "Extended master secret full handshake check");

    }
}

sub setrmextms($$)
{
    ($crmextms, $srmextms) = @_;
}

sub clearall()
{
    $cextms = 0;
    $sextms = 0;
    $fullhand = 0;
    $proxy->clear();
}
