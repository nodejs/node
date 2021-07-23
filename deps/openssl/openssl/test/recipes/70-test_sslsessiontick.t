#! /usr/bin/env perl
# Copyright 2015-2018 The OpenSSL Project Authors. All Rights Reserved.
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

my $test_name = "test_sslsessiontick";
setup($test_name);

plan skip_all => "TLSProxy isn't usable on $^O"
    if $^O =~ /^(VMS)$/;

plan skip_all => "$test_name needs the dynamic engine feature enabled"
    if disabled("engine") || disabled("dynamic-engine");

plan skip_all => "$test_name needs the sock feature enabled"
    if disabled("sock");

plan skip_all => "$test_name needs SSLv3, TLSv1, TLSv1.1 or TLSv1.2 enabled"
    if alldisabled(("ssl3", "tls1", "tls1_1", "tls1_2"));

$ENV{OPENSSL_ia32cap} = '~0x200000200000000';

sub checkmessages($$$$$$);
sub clearclient();
sub clearall();

my $chellotickext = 0;
my $shellotickext = 0;
my $fullhand = 0;
my $ticketseen = 0;

my $proxy = TLSProxy::Proxy->new(
    undef,
    cmdstr(app(["openssl"]), display => 1),
    srctop_file("apps", "server.pem"),
    (!$ENV{HARNESS_ACTIVE} || $ENV{HARNESS_VERBOSE})
);

#Test 1: By default with no existing session we should get a session ticket
#Expected result: ClientHello extension seen; ServerHello extension seen
#                 NewSessionTicket message seen; Full handshake
$proxy->clientflags("-no_tls1_3");
$proxy->start() or plan skip_all => "Unable to start up Proxy for tests";
plan tests => 10;
checkmessages(1, "Default session ticket test", 1, 1, 1, 1);

#Test 2: If the server does not accept tickets we should get a normal handshake
#with no session tickets
#Expected result: ClientHello extension seen; ServerHello extension not seen
#                 NewSessionTicket message not seen; Full handshake
clearall();
$proxy->clientflags("-no_tls1_3");
$proxy->serverflags("-no_ticket");
$proxy->start();
checkmessages(2, "No server support session ticket test", 1, 0, 0, 1);

#Test 3: If the client does not accept tickets we should get a normal handshake
#with no session tickets
#Expected result: ClientHello extension not seen; ServerHello extension not seen
#                 NewSessionTicket message not seen; Full handshake
clearall();
$proxy->clientflags("-no_tls1_3 -no_ticket");
$proxy->start();
checkmessages(3, "No client support session ticket test", 0, 0, 0, 1);

#Test 4: Test session resumption with session ticket
#Expected result: ClientHello extension seen; ServerHello extension not seen
#                 NewSessionTicket message not seen; Abbreviated handshake
clearall();
(undef, my $session) = tempfile();
$proxy->serverconnects(2);
$proxy->clientflags("-no_tls1_3 -sess_out ".$session);
$proxy->start();
$proxy->clearClient();
$proxy->clientflags("-no_tls1_3 -sess_in ".$session);
$proxy->clientstart();
checkmessages(4, "Session resumption session ticket test", 1, 0, 0, 0);
unlink $session;

#Test 5: Test session resumption with ticket capable client without a ticket
#Expected result: ClientHello extension seen; ServerHello extension seen
#                 NewSessionTicket message seen; Abbreviated handshake
clearall();
(undef, $session) = tempfile();
$proxy->serverconnects(2);
$proxy->clientflags("-no_tls1_3 -sess_out ".$session." -no_ticket");
$proxy->start();
$proxy->clearClient();
$proxy->clientflags("-no_tls1_3 -sess_in ".$session);
$proxy->clientstart();
checkmessages(5, "Session resumption with ticket capable client without a "
                 ."ticket", 1, 1, 1, 0);
unlink $session;

#Test 6: Client accepts empty ticket.
#Expected result: ClientHello extension seen; ServerHello extension seen;
#                 NewSessionTicket message seen; Full handshake.
clearall();
$proxy->filter(\&ticket_filter);
$proxy->clientflags("-no_tls1_3");
$proxy->start();
checkmessages(6, "Empty ticket test",  1, 1, 1, 1);

#Test 7-8: Client keeps existing ticket on empty ticket.
clearall();
(undef, $session) = tempfile();
$proxy->serverconnects(3);
$proxy->filter(undef);
$proxy->clientflags("-no_tls1_3 -sess_out ".$session);
$proxy->start();
$proxy->clearClient();
$proxy->clientflags("-no_tls1_3 -sess_in ".$session." -sess_out ".$session);
$proxy->filter(\&inject_empty_ticket_filter);
$proxy->clientstart();
#Expected result: ClientHello extension seen; ServerHello extension seen;
#                 NewSessionTicket message seen; Abbreviated handshake.
checkmessages(7, "Empty ticket resumption test",  1, 1, 1, 0);
clearclient();
$proxy->clientflags("-no_tls1_3 -sess_in ".$session);
$proxy->filter(undef);
$proxy->clientstart();
#Expected result: ClientHello extension seen; ServerHello extension not seen;
#                 NewSessionTicket message not seen; Abbreviated handshake.
checkmessages(8, "Empty ticket resumption test",  1, 0, 0, 0);
unlink $session;

#Test 9: Bad server sends the ServerHello extension but does not send a
#NewSessionTicket
#Expected result: Connection failure
clearall();
$proxy->clientflags("-no_tls1_3");
$proxy->serverflags("-no_ticket");
$proxy->filter(\&inject_ticket_extension_filter);
$proxy->start();
ok(TLSProxy::Message->fail, "Server sends ticket extension but no ticket test");

#Test10: Bad server does not send the ServerHello extension but does send a
#NewSessionTicket
#Expected result: Connection failure
clearall();
$proxy->clientflags("-no_tls1_3");
$proxy->serverflags("-no_ticket");
$proxy->filter(\&inject_empty_ticket_filter);
$proxy->start();
ok(TLSProxy::Message->fail, "No server ticket extension but ticket sent test");

sub ticket_filter
{
    my $proxy = shift;

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_NEW_SESSION_TICKET) {
            $message->ticket("");
            $message->repack();
        }
    }
}

sub inject_empty_ticket_filter {
    my $proxy = shift;

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_NEW_SESSION_TICKET) {
            # Only inject the message first time we're called.
            return;
        }
    }

    my @new_message_list = ();
    foreach my $message (@{$proxy->message_list}) {
        push @new_message_list, $message;
        if ($message->mt == TLSProxy::Message::MT_SERVER_HELLO) {
            $message->set_extension(TLSProxy::Message::EXT_SESSION_TICKET, "");
            $message->repack();
            # Tack NewSessionTicket onto the ServerHello record.
            # This only works if the ServerHello is exactly one record.
            my $record = ${$message->records}[0];

            my $offset = $message->startoffset + $message->encoded_length;
            my $newsessionticket = TLSProxy::NewSessionTicket->new(
                1, "", [$record], $offset, []);
            $newsessionticket->repack();
            push @new_message_list, $newsessionticket;
        }
    }
    $proxy->message_list([@new_message_list]);
}

sub inject_ticket_extension_filter
{
    my $proxy = shift;

    # We're only interested in the initial ServerHello
    if ($proxy->flight != 1) {
        return;
    }

    foreach my $message (@{$proxy->message_list}) {
        if ($message->mt == TLSProxy::Message::MT_SERVER_HELLO) {
            #Add the session ticket extension to the ServerHello even though
            #we are not going to send a NewSessionTicket message
            $message->set_extension(TLSProxy::Message::EXT_SESSION_TICKET, "");

            $message->repack();
        }
    }
}

sub checkmessages($$$$$$)
{
    my ($testno, $testname, $testch, $testsh, $testtickseen, $testhand) = @_;

    subtest $testname => sub {

	foreach my $message (@{$proxy->message_list}) {
	    if ($message->mt == TLSProxy::Message::MT_CLIENT_HELLO
                || $message->mt == TLSProxy::Message::MT_SERVER_HELLO) {
		#Get the extensions data
		my %extensions = %{$message->extension_data};
		if (defined
                    $extensions{TLSProxy::Message::EXT_SESSION_TICKET}) {
		    if ($message->mt == TLSProxy::Message::MT_CLIENT_HELLO) {
			$chellotickext = 1;
		    } else {
			$shellotickext = 1;
		    }
		}
	    } elsif ($message->mt == TLSProxy::Message::MT_CERTIFICATE) {
		#Must be doing a full handshake
		$fullhand = 1;
	    } elsif ($message->mt == TLSProxy::Message::MT_NEW_SESSION_TICKET) {
		$ticketseen = 1;
	    }
	}

	plan tests => 5;

	ok(TLSProxy::Message->success, "Handshake");
	ok(($testch && $chellotickext) || (!$testch && !$chellotickext),
	   "ClientHello extension Session Ticket check");
	ok(($testsh && $shellotickext) || (!$testsh && !$shellotickext),
	   "ServerHello extension Session Ticket check");
	ok(($testtickseen && $ticketseen) || (!$testtickseen && !$ticketseen),
	   "Session Ticket message presence check");
	ok(($testhand && $fullhand) || (!$testhand && !$fullhand),
	   "Session Ticket full handshake check");
    }
}


sub clearclient()
{
    $chellotickext = 0;
    $shellotickext = 0;
    $fullhand = 0;
    $ticketseen = 0;
    $proxy->clearClient();
}

sub clearall()
{
    clearclient();
    $proxy->clear();
}
