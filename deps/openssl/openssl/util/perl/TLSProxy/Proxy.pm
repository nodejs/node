# Copyright 2016-2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use POSIX ":sys_wait_h";

package TLSProxy::Proxy;

use File::Spec;
use IO::Socket;
use IO::Select;
use TLSProxy::Record;
use TLSProxy::Message;
use TLSProxy::ClientHello;
use TLSProxy::ServerHello;
use TLSProxy::HelloVerifyRequest;
use TLSProxy::EncryptedExtensions;
use TLSProxy::Certificate;
use TLSProxy::CertificateRequest;
use TLSProxy::CertificateVerify;
use TLSProxy::ServerKeyExchange;
use TLSProxy::NewSessionTicket;
use TLSProxy::NextProto;

my $have_IPv6;
my $useINET6;
my $IP_factory;

BEGIN
{
    # IO::Socket::IP is on the core module list, IO::Socket::INET6 isn't.
    # However, IO::Socket::INET6 is older and is said to be more widely
    # deployed for the moment, and may have less bugs, so we try the latter
    # first, then fall back on the core modules.  Worst case scenario, we
    # fall back to IO::Socket::INET, only supports IPv4.
    eval {
        require IO::Socket::INET6;
        my $s = IO::Socket::INET6->new(
            LocalAddr => "::1",
            LocalPort => 0,
            Listen=>1,
            );
        $s or die "\n";
        $s->close();
    };
    if ($@ eq "") {
        $IP_factory = sub { IO::Socket::INET6->new(Domain => AF_INET6, @_); };
        $have_IPv6 = 1;
        $useINET6 = 1;
    } else {
        eval {
            require IO::Socket::IP;
            my $s = IO::Socket::IP->new(
                LocalAddr => "::1",
                LocalPort => 0,
                Listen=>1,
                );
            $s or die "\n";
            $s->close();
        };
        if ($@ eq "") {
            $IP_factory = sub { IO::Socket::IP->new(@_); };
            $have_IPv6 = 1;
            $useINET6 = 0;
        } else {
            $IP_factory = sub { IO::Socket::INET->new(@_); };
            $have_IPv6 = 0;
            $useINET6 = 0;
        }
    }
}

my $is_tls13 = 0;
my $ciphersuite = undef;

sub new {
    my $class = shift;
    my ($filter,
        $execute,
        $cert,
        $debug,
        $use_IPv6) = @_;
    return init($class, $filter, $execute, $cert, $debug, 0, $use_IPv6);
}

sub new_dtls {
    my $class = shift;
    my ($filter,
        $execute,
        $cert,
        $debug,
        $use_IPv6) = @_;
    return init($class, $filter, $execute, $cert, $debug, 1, $use_IPv6);
}

sub init
{
    my $class = shift;
    my ($filter,
        $execute,
        $cert,
        $debug,
        $isdtls,
        $use_IPv6) = @_;
    $use_IPv6 //= $have_IPv6;

    my $self = {
        #Public read/write
        proxy_addr => $use_IPv6 ? "[::1]" : "127.0.0.1",
        filter => $filter,
        serverflags => "",
        clientflags => "",
        serverconnects => 1,
        reneg => 0,
        sessionfile => undef,
        expected_tickets => 2,

        #Public read
        isdtls => $isdtls,
        proxy_port => 0,
        server_port => 0,
        serverpid => 0,
        clientpid => 0,
        clientexit => 0,
        execute => $execute,
        cert => $cert,
        debug => $debug,
        cipherc => "",
        ciphersuitesc => "",
        ciphers => "AES128-SHA",
        ciphersuitess => "TLS_AES_128_GCM_SHA256",
        flight => -1,
        direction => -1,
        partial => ["", ""],
        record_list => [],
        message_list => [],
        seen_msgseq => {},
    };

    return bless $self, $class;
}

sub DESTROY
{
    my $self = shift;

    $self->{proxy_sock}->close() if $self->{proxy_sock};
}

sub clearClient
{
    my $self = shift;

    $self->{cipherc} = "";
    $self->{ciphersuitec} = "";
    $self->{flight} = -1;
    $self->{direction} = -1;
    $self->{partial} = ["", ""];
    $self->{record_list} = [];
    $self->{message_list} = [];
    $self->{seen_msgseq} = {};
    $self->{clientflags} = "";
    $self->{sessionfile} = undef;
    $self->{expected_tickets} = 2;
    $self->{clientpid} = 0;
    $self->{clientexit} = 0;
    $is_tls13 = 0;
    $ciphersuite = undef;

    TLSProxy::Message->clear();
    TLSProxy::Record->clear();
}

sub clear
{
    my $self = shift;

    $self->clearClient;
    $self->{ciphers} = "AES128-SHA";
    $self->{ciphersuitess} = "TLS_AES_128_GCM_SHA256";
    $self->{serverflags} = "";
    $self->{serverconnects} = 1;
    $self->{serverpid} = 0;
    $self->{reneg} = 0;
}

sub restart
{
    my $self = shift;

    $self->clear;
    $self->start;
}

sub clientrestart
{
    my $self = shift;

    $self->clear;
    $self->clientstart;
}

sub connect_to_server
{
    my $self = shift;
    my $servaddr = $self->{server_addr};

    $servaddr =~ s/[\[\]]//g; # Remove [ and ]

    my $sock = $IP_factory->(PeerAddr => $servaddr,
                             PeerPort => $self->{server_port},
                             Proto => $self->{isdtls} ? 'udp' : 'tcp');
    if (!defined($sock)) {
        my $err = $!;
        kill(3, $self->{real_serverpid});
        die "unable to connect: $err\n";
    }

    $self->{server_sock} = $sock;
}

sub start
{
    my ($self) = shift;
    my $pid;

    #
    # s390x is a somewhat special case here.  It uses hw acceleration under
    # the covers when computing MACs, and in so doing avoids the use of the
    # needed ossltest provider when computing the underlying digest.  Since
    # TLSProxy needs the ossltest provider to compute reliable known data in
    # the digest, we disable MAC hw acceleration here to ensure that the provider
    # gets used, just as it does with other architectures.
    #
    $ENV{OPENSSL_s390xcap} = "kmac:~0:~f000";

    # Create the Proxy socket
    my $proxaddr = $self->{proxy_addr};
    $proxaddr =~ s/[\[\]]//g; # Remove [ and ]

    my @proxyargs;

    if ($self->{isdtls}) {
        # The socket is left unconnected: the client's address and port are
        # learned from the first datagram it sends and remembered for
        # sending back the server's flights.  That way the client's port is
        # picked (race free) by the kernel rather than by us.
        @proxyargs = (
            LocalHost   => $proxaddr,
            LocalPort   => 0,
            Proto       => "udp",
        );
    } else {
        @proxyargs = (
            LocalHost   => $proxaddr,
            LocalPort   => 0,
            Proto       => "tcp",
            Listen      => SOMAXCONN,
        );
    }

    if (my $sock = $IP_factory->(@proxyargs)) {
        $self->{proxy_sock} = $sock;
        $self->{proxy_port} = $sock->sockport();
        $self->{proxy_addr} = $sock->sockhost();
        $self->{proxy_addr} =~ s/(.*:.*)/[$1]/;
        print "Proxy started on port ",
            "$self->{proxy_addr}:$self->{proxy_port}\n";
        # use same address for s_server
        $self->{server_addr} = $self->{proxy_addr};
    } else {
        warn "Failed creating proxy socket (".$proxaddr.",0): $!\n";
    }

    if ($self->{proxy_sock} == 0) {
        return 0;
    }

    my $execcmd = $self->execute
        ." s_server -no_comp -provider=p_ossltest -provider=default -propquery ?provider=p_ossltest -state"
        #In TLSv1.3 we issue two session tickets. The default session id
        #callback gets confused because the ossltest provider causes the same
        #session id to be created twice due to the changed random number
        #generation. Using "-ext_cache" replaces the default callback with a
        #different one that doesn't get confused.
        ." -ext_cache"
        ." -accept $self->{server_addr}:0"
        ." -naccept ".$self->serverconnects;
    if (defined $self->cert) {
        $execcmd .= " -cert ".$self->cert." -cert2 ".$self->cert;
    } else {
        $execcmd .= " -nocert";
    }

    if ($self->{isdtls}) {
        $execcmd .= " -dtls -max_protocol DTLSv1.3"
                    # TLSProxy does not support message fragmentation. So
                    # set a high mtu and fingers crossed.
                    ." -mtu 1500";
    } else {
        $execcmd .= " -rev -max_protocol TLSv1.3";
    }
    if ($self->ciphers ne "") {
        $execcmd .= " -cipher ".$self->ciphers;
    }
    if ($self->ciphersuitess ne "") {
        $execcmd .= " -ciphersuites ".$self->ciphersuitess;
    }
    if ($self->serverflags ne "") {
        $execcmd .= " ".$self->serverflags;
    }
    if ($self->debug) {
        print STDERR "Server command: $execcmd\n";
    }

    open(my $savedin, "<&STDIN");

    # DTLS s_server exits when its stdin reaches EOF, so it needs one that
    # stays open for as long as the test does.  Give it the read end of a
    # pipe and keep the write end here; closing that in clientstart() is
    # what lets it finish.
    my $stdin_holder;
    if ($self->{isdtls}) {
        my $rd;

        # A previous run that died before its teardown may have left one.
        close($self->{stdin_holder}) if defined($self->{stdin_holder});
        pipe($rd, $stdin_holder) or die "Failed to create stdin pipe: $!\n";
        open(STDIN, "<&", $rd) or die "Failed to replace STDIN: $!\n";
        close($rd);
    }
    $pid = open(STDIN, "$execcmd 2>&1 |") or die "Failed to $execcmd: $!\n";
    $self->{real_serverpid} = $pid;
    $self->{stdin_holder} = $stdin_holder;

    # Process the output from s_server until we find the ACCEPT line, which
    # tells us what the accepting address and port are.
    while (<>) {
        print STDERR $_;
        s/\R$//;                # Better chomp
        next unless (/^ACCEPT\s.*:(\d+)$/);
        $self->{server_port} = $1;
        last;
    }

    if ($self->{server_port} == 0) {
        # This actually means that s_server exited, because otherwise
        # we would still searching for ACCEPT...
        waitpid($pid, 0);
        die "no ACCEPT detected in '$execcmd' output: $?\n";
    }

    # Just make sure everything else is simply printed [as separate lines].
    # The sub process simply inherits our STD* and will keep consuming
    # server's output and printing it as long as there is anything there,
    # out of our way.
    my $error;
    $pid = undef;
    if (eval { require Win32::Process; 1; }) {
        if (Win32::Process::Create(my $h, $^X, 'perl -ne "print STDERR $_"', 0, 0, ".")) {
            $pid = $h->GetProcessID();
            $self->{proc_handle} = $h;  # hold handle till next round [or exit]
        } else {
            $error = Win32::FormatMessage(Win32::GetLastError());
        }
    } else {
        if (defined($pid = fork)) {
            $pid or exec($^X, '-ne', 'print STDERR $_') or exit($!);
        } else {
            $error = $!;
        }
    }

    # Change back to original stdin
    open(STDIN, "<&", $savedin);
    close($savedin);

    if (!defined($pid)) {
        kill(3, $self->{real_serverpid});
        die "Failed to capture s_server's output: $error\n";
    }

    $self->{serverpid} = $pid;

    print STDERR "Server responds on ",
                 "$self->{server_addr}:$self->{server_port}\n";

    # Connect right away...
    $self->connect_to_server();

    return $self->clientstart;
}

sub clientstart
{
    my ($self) = shift;

    my $success = 1;

    if ($self->execute) {
        my $pid;
        my $execcmd = $self->execute
             ." s_client -provider=p_ossltest -provider=default -propquery ?provider=p_ossltest"
             ." -state -connect $self->{proxy_addr}:$self->{proxy_port}";
        if ($self->{isdtls}) {
            $execcmd .= " -dtls -max_protocol DTLSv1.3"
                        # TLSProxy does not support message fragmentation. So
                        # set a high mtu and fingers crossed.
                        ." -mtu 1500";
        } else {
            $execcmd .= " -max_protocol TLSv1.3";
        }
        if ($self->cipherc ne "") {
            $execcmd .= " -cipher ".$self->cipherc;
        }
        if ($self->ciphersuitesc ne "") {
            $execcmd .= " -ciphersuites ".$self->ciphersuitesc;
        }
        if ($self->clientflags ne "") {
            $execcmd .= " ".$self->clientflags;
        }
        if ($self->clientflags !~ m/-(no)?servername/) {
            $execcmd .= " -servername localhost";
        }
        if (defined $self->sessionfile) {
            $execcmd .= " -ign_eof";
        }
        if ($self->debug) {
            print STDERR "Client command: $execcmd\n";
        }
        open(my $savedout, ">&STDOUT");
        # If we open pipe with new descriptor, attempt to close it,
        # explicitly or implicitly, would incur waitpid and effectively
        # dead-lock...
        if (!($pid = open(STDOUT, "| $execcmd"))) {
            my $err = $!;
            kill(3, $self->{real_serverpid});
            die "Failed to $execcmd: $err\n";
        }
        $self->{clientpid} = $pid;

        # queue [magic] input
        print $self->reneg ? "R" : "test";

        # this closes client's stdin without waiting for its pid
        open(STDOUT, ">&", $savedout);
        close($savedout);
    }

    # Wait for incoming connection from client
    my $fdset = IO::Select->new($self->{proxy_sock});
    if (!$fdset->can_read(60)) {
        kill(3, $self->{real_serverpid});
        die "s_client didn't try to connect\n";
    }

    my $client_sock;
    if($self->{isdtls}) {
        # The proxy socket is unconnected; the client's address is learned
        # from the datagrams it sends (see client_sockaddr below).  A new
        # s_client (with a fresh kernel-assigned port) may connect on each
        # clientstart(), so forget any previous peer.
        $client_sock = $self->{proxy_sock};
        $self->{client_sockaddr} = undef;
    } elsif (!($client_sock = $self->{proxy_sock}->accept())) {
        warn "Failed accepting incoming connection: $!\n";
        return 0;
    }

    print "Connection opened\n";

    my $server_sock = $self->{server_sock};
    my $indata;

    #Wait for either the server socket or the client socket to become readable
    $fdset = IO::Select->new($server_sock, $client_sock);
    my @ready;
    my $ctr = 0;
    local $SIG{PIPE} = "IGNORE";
    $self->{saw_session_ticket} = undef;
    $self->{session_ticket_seq} = [];
    $self->{saw_session_ticket_ack} = {};
    $self->{server_epoch} = 0;
    $self->{server_sequence_number} = 0;
    $self->{client_epoch} = 0;
    $self->{client_sequence_number} = 0;

    while($fdset->count && $ctr < 50) {
        if (defined($self->{sessionfile})) {
            # s_client got -ign_eof and won't be exiting voluntarily, so we
            # look for data *and* session ticket...
            last if TLSProxy::Message->success()
                    && $self->handshake_complete() == 1;
        }

        # For DTLS, check exit conditions BEFORE calling can_read/sysread
        # to avoid blocking on a socket where the peer has closed.
        # Once we have the session ticket and have seen the end of the
        # message stream (close_notify), we're done.
        if ($self->{isdtls}) {
            my $success_flag = TLSProxy::Message->success();
            my $handshake_done = $self->handshake_complete();
            my $msg_end = TLSProxy::Message->end();
            if (($success_flag && $handshake_done == 1) || ($handshake_done == 1 && $msg_end)) {
                last;
            }
        }

        if (!(@ready = $fdset->can_read(0.1))) {
            my $success_flag = TLSProxy::Message->success();
            my $handshake_done = $self->handshake_complete();
            my $msg_end = TLSProxy::Message->end();
            if ($success_flag && $handshake_done == 1) {
                last;
            }
            $ctr++;
            next;
        }
        foreach my $hand (@ready) {
            if ($hand == $server_sock) {
                if ($server_sock->sysread($indata, 16384)) {
                    if ($indata = $self->process_packet(1, $indata)) {
                        if (!$self->client_syswrite($client_sock, $indata)) {
                            # For DTLS/UDP, syswrite failure after handshake completion
                            # is not necessarily an error - the client may have already
                            # sent close_notify and exited. Unlike TCP, UDP is
                            # connectionless so we can't rely on socket state.
                            if (!$self->{isdtls} || $self->handshake_complete() != 1) {
                                goto END;
                            }
                        }
                    }
                    $ctr = 0;
                } else {
                    # For DTLS/UDP, sysread returning 0 doesn't mean the connection
                    # is closed like it does for TCP. For TCP, 0 means EOF/FIN
                    # received. For UDP, it may just mean no data available.
                    # Skip the shutdown logic for DTLS to avoid prematurely
                    # closing the connection.
                    if (!$self->{isdtls}) {
                        $fdset->remove($server_sock);
                        $client_sock->shutdown(SHUT_WR);
                    }
                }
            } elsif ($hand == $client_sock) {
                if ($self->client_sysread($client_sock, \$indata)) {
                    if ($indata = $self->process_packet(0, $indata)) {
                        if (!$server_sock->syswrite($indata)) {
                            # For DTLS/UDP, syswrite failure after handshake completion
                            # is not necessarily an error - the server may have already
                            # sent close_notify and exited. Unlike TCP, UDP is
                            # connectionless so we can't rely on socket state.
                            if (!$self->{isdtls} || $self->handshake_complete() != 1) {
                                goto END;
                            }
                        }
                    }
                    $ctr = 0;
                } else {
                    # For DTLS/UDP, sysread returning 0 doesn't mean the connection
                    # is closed like it does for TCP. For TCP, 0 means EOF/FIN
                    # received. For UDP, it may just mean no data available.
                    # Skip the shutdown logic for DTLS to avoid prematurely
                    # closing the connection.
                    if (!$self->{isdtls}) {
                        $fdset->remove($client_sock);
                        $server_sock->shutdown(SHUT_WR);
                    }
                }
            } else {
                kill(3, $self->{real_serverpid});
                die "Unexpected handle";
            }
        }
    }

    if ($ctr >= 50) {
        kill(3, $self->{real_serverpid});
        print "No progress made\n";
        $success = 0;
    }

    END:
    print "Connection closed\n";
    if($server_sock) {
        if ($self->{isdtls} && $self->is_tls13()) {
            my $alert_message = $self->construct_alert_message($self->{client_epoch}, $self->{client_sequence_number} + 1);
            $server_sock->syswrite($alert_message) or warn "Failed to send close_notify alert: $!\n";
        }
        $server_sock->close();
        $self->{server_sock} = undef;
    }
    if($client_sock) {
        # For DTLSv1.3 tests that are using sessionfile we need to send a close_notify
        # this is because closing the socket does not result in a FIN being sent as in TCP.
        if ($self->{isdtls} && $self->is_tls13() && defined($self->{sessionfile})) {
            my $alert_message = $self->construct_alert_message($self->{server_epoch}, $self->{server_sequence_number} + 1);
            $self->client_syswrite($client_sock, $alert_message)
                or warn "Failed to send close_notify alert: $!\n";
        }

        #Closing this also kills the child process
        if (!$self->{isdtls}) {
            $client_sock->close();
        }
    }

    my $pid;
    if (--$self->{serverconnects} == 0) {
        # Let a DTLS s_server see EOF on its stdin, so that it exits and the
        # sink process reading its output finishes too.  This has to happen
        # before either is waited for.
        if (defined($self->{stdin_holder})) {
            close($self->{stdin_holder});
            $self->{stdin_holder} = undef;
        }
        $pid = $self->{serverpid};
        print "Waiting for 'perl -ne print' process to close: $pid...\n";
        $pid = waitpid($pid, 0);
        if ($pid > 0) {
            die "exit code $? from 'perl -ne print' process\n" if $? != 0;
        } elsif ($pid == 0) {
            kill(3, $self->{real_serverpid});
            die "lost control over $self->{serverpid}?";
        }
        $pid = $self->{real_serverpid};
        print "Waiting for s_server process to close: $pid...\n";
        # it's done already, just collect the exit code [and reap]...
        waitpid($pid, 0);

        die "exit code $? from s_server process\n" if $? != 0;
    } else {
        # It's a bit counter-intuitive spot to make next connection to
        # the s_server. Rationale is that established connection works
        # as synchronization point, in sense that this way we know that
        # s_server is actually done with current session...
        $self->connect_to_server();
    }
    $pid = $self->{clientpid};
    print "Waiting for s_client process to close: $pid...\n";
    waitpid($pid, 0);
    $self->{clientexit} = $?;

    return $success;
}

# Read data sent by the client.  For DTLS the proxy socket is unconnected,
# so recv() is used and the sender's address is remembered as the
# destination for datagrams sent back to the client.
sub client_sysread
{
    my ($self, $client_sock, $dataref) = @_;

    if (!$self->{isdtls}) {
        return $client_sock->sysread($$dataref, 16384);
    }

    my $peer = $client_sock->recv($$dataref, 16384, 0);
    return undef if !defined $peer;
    $self->{client_sockaddr} = $peer;
    return length($$dataref);
}

# Send data to the client, using the address it last sent from in the DTLS
# case.
sub client_syswrite
{
    my ($self, $client_sock, $data) = @_;

    if (!$self->{isdtls}) {
        return $client_sock->syswrite($data);
    }

    if (!defined $self->{client_sockaddr}) {
        warn "Cannot send to client: no datagram received from it yet\n";
        return undef;
    }

    return $client_sock->send($data, 0, $self->{client_sockaddr});
}

sub construct_alert_message
{
    my ($self, $epoch, $sequence_number) = @_;

    die "construct_alert_message only valid for DTLSv1.3 tests\n"
        if !$self->{isdtls} || !$self->is_tls13();

    my $alert_level = pack("C", 1);                # Warning (1)
    my $alert_description = pack("C", 0);          # close_notify (0)
    my $alert_payload = $alert_level.$alert_description;

    if ($epoch == 0) {
        # Epoch 0 uses DTLSPlaintext.
        my $seqhi = ($sequence_number >> 32) & 0xffff;
        my $seqmi = ($sequence_number >> 16) & 0xffff;
        my $seqlo = ($sequence_number >> 0) & 0xffff;

        return pack("C", 21).                 # Alert (21)
               pack("n", 0xFEFD).             # legacy version DTLS 1.2
               pack("n", $epoch).
               pack('nnn', $seqhi, $seqmi, $seqlo).
               pack("n", length($alert_payload)).
               $alert_payload;
    }

    # Keyed epochs use DTLSCiphertext. The p_ossltest cipher leaves the
    # payload unchanged and does not verify the tag. The inner plaintext
    # holds close_notify and its content type; a zero tag follows.
    # The header uses a 16 bit sequence number and a length field. Mask the
    # sequence number with the first two payload bytes, as in Record.pm.
    my $payload = $alert_payload.pack("C", 21).("\x00" x 16);
    my $maskhi = unpack("n", substr($payload, 0, 2));
    my $seqlo = ($sequence_number & 0xffff) ^ $maskhi;
    my $first = 0x20 | 0x08 | 0x04 | ($epoch & 0x03);

    return pack("C", $first).
           pack("n", $seqlo).
           pack("n", length($payload)).
           $payload;
}

sub process_packet
{
    my ($self, $serverissender, $packet) = @_;

    if ($serverissender) {
        print "Received server packet\n";
    } else {
        print "Received client packet\n";
    }

    if ($self->{direction} != $serverissender) {
        $self->{flight} = $self->{flight} + 1;
        $self->{direction} = $serverissender;
    }

    print "Packet length = ".length($packet)."\n";
    print "Processing flight ".$self->flight."\n";

    #Return contains the list of record found in the packet followed by the
    #list of messages in those records and any partial message
    my @ret = TLSProxy::Record->get_records($serverissender, $self->flight,
                                            $self->{partial}[$serverissender].$packet,
                                            $self->{isdtls});

    $self->{partial}[$serverissender] = $ret[2];
    push @{$self->{record_list}}, @{$ret[0]};
    if ($self->{isdtls}) {
        foreach my $msg (@{$ret[1]}) {
            my $key = $msg->server . ":" . $msg->msgseq;
            push @{$self->{message_list}}, $msg
                unless $self->{seen_msgseq}{$key}++;
        }
    } else {
        push @{$self->{message_list}}, @{$ret[1]};
    }

    print "\n";

    if (scalar(@{$ret[0]}) == 0 or length($ret[2]) != 0) {
        return "";
    }

    #Finished parsing. Call user provided filter here
    if (defined $self->filter) {
        $self->filter->($self);
    }

    #Take a note on NewSessionTicket
    foreach my $message (reverse @{$self->{message_list}}) {
        if ($message->{mt} == TLSProxy::Message::MT_NEW_SESSION_TICKET) {
            $self->{saw_session_ticket} = 1;
            # Obtain the most recent sequence number of the record
            # that contained the NewSessionTicket message
            if ($self->{isdtls} && $self->is_tls13()) {
                foreach my $record (@{$message->{records}}) {
                    if (@{$self->{session_ticket_seq}} == 0) {
                        push @{$self->{session_ticket_seq}}, TLSProxy::RecordNumber->new($record->epoch, $record->seq);
                    } elsif (scalar(@{$self->{session_ticket_seq}}) != $self->{expected_tickets}) {
                        my $match = $self->find_session_ticket_ack($record->epoch, $record->seq);
                        if ($match == -1) {
                            push @{$self->{session_ticket_seq}}, TLSProxy::RecordNumber->new($record->epoch, $record->seq);
                        }
                    }
                }
            }
            last;
        }
    }

    #Reconstruct the packet
    $packet = "";
    foreach my $record (@{$self->record_list}) {
        $packet .= $record->reconstruct_record($serverissender);

        # After we have set the saw_session_ticket flag, we can check if we have
        # seen a session ticket ack. This is only relevant for DTLSv1.3
        if ($self->{isdtls} && $self->is_tls13()) {
            $self->seen_session_ticket_ack($record);

            my $epoch_key = $serverissender ? 'server_epoch' : 'client_epoch';
            my $seq_key = $serverissender ? 'server_sequence_number' : 'client_sequence_number';
            my $rec_epoch = $record->epoch();
            my $rec_seq = $record->seq();

            if ($rec_epoch > $self->{$epoch_key}) {
                $self->{$epoch_key} = $rec_epoch;
                $self->{$seq_key} = $rec_seq;
            } elsif ($rec_epoch == $self->{$epoch_key} && $rec_seq > $self->{$seq_key}) {
                $self->{$seq_key} = $rec_seq;
            }
        }
    }

    print "Forwarded packet length = ".length($packet)."\n\n";

    return $packet;
}

sub seen_session_ticket_ack
{
    my $self = shift;
    my $record = shift;

    my $ack_hash = $self->{saw_session_ticket_ack};
    return if !$self->{saw_session_ticket}
              || scalar(keys %{$ack_hash}) == $self->{expected_tickets};
    return if $record->content_type() != TLSProxy::Record::RT_ACK;

    my @record_numbers = ();
    $record->get_actual_acked_record_numbers(\@record_numbers);
    my $ticket_seq = $self->{session_ticket_seq};

    foreach my $record_number (@record_numbers) {
        my $epoch = $record_number->epoch();
        my $seqnum = $record_number->seqnum();
        my $key = "$epoch:$seqnum";
        next if exists $ack_hash->{$key};

        my $match = $self->find_session_ticket_ack($epoch, $seqnum);

        if ($match != -1) {
            my $session_ticket = splice(@{$ticket_seq}, $match, 1);
            $ack_hash->{$key} = $session_ticket;
            last if scalar(keys %{$ack_hash}) == $self->{expected_tickets};
        }
    }
}

sub find_session_ticket_ack
{
    my $self = shift;
    my $record_number_epoch = shift;
    my $record_number_seqnum = shift;

    my $tickets = $self->{session_ticket_seq};
    for (my $i = 0; $i < @{$tickets}; $i++) {
        my $ticket = $tickets->[$i];
        if ($record_number_epoch == $ticket->epoch() &&
            $record_number_seqnum == $ticket->seqnum()) {
            return $i;
        }
    }

    return -1;
}

sub handshake_complete
{
    my $self = shift;
    my $res = 0;

    if ($self->{isdtls} && $self->is_tls13() && defined($self->{sessionfile})) {
        # The handshake is complete once every expected NewSessionTicket
        # (2 by default, but only 1 follows a resumption) has been acked
        if (scalar(keys %{$self->{saw_session_ticket_ack}}) == $self->{expected_tickets}) {
            $res = 1;
        }
    } else {
        if ($self->{saw_session_ticket}) {
            $res = 1;
        }
    }

    return $res;
}

#Read accessors
sub execute
{
    my $self = shift;
    return $self->{execute};
}
sub cert
{
    my $self = shift;
    return $self->{cert};
}
sub debug
{
    my $self = shift;
    return $self->{debug};
}
sub flight
{
    my $self = shift;
    return $self->{flight};
}
sub record_list
{
    my $self = shift;
    return $self->{record_list};
}
sub success
{
    my $self = shift;
    return $self->{success};
}
sub end
{
    my $self = shift;
    return $self->{end};
}
sub supports_IPv6
{
    my $self = shift;
    return $have_IPv6;
}
sub proxy_addr
{
    my $self = shift;
    return $self->{proxy_addr};
}
sub proxy_port
{
    my $self = shift;
    return $self->{proxy_port};
}
sub server_addr
{
    my $self = shift;
    return $self->{server_addr};
}
sub server_port
{
    my $self = shift;
    return $self->{server_port};
}
sub serverpid
{
    my $self = shift;
    return $self->{serverpid};
}
sub clientpid
{
    my $self = shift;
    return $self->{clientpid};
}
sub clientexit
{
    my $self = shift;
    return $self->{clientexit};
}

#Read/write accessors
sub filter
{
    my $self = shift;
    if (@_) {
        $self->{filter} = shift;
    }
    return $self->{filter};
}
sub cipherc
{
    my $self = shift;
    if (@_) {
        $self->{cipherc} = shift;
    }
    return $self->{cipherc};
}
sub ciphersuitesc
{
    my $self = shift;
    if (@_) {
        $self->{ciphersuitesc} = shift;
    }
    return $self->{ciphersuitesc};
}
sub ciphers
{
    my $self = shift;
    if (@_) {
        $self->{ciphers} = shift;
    }
    return $self->{ciphers};
}
sub ciphersuitess
{
    my $self = shift;
    if (@_) {
        $self->{ciphersuitess} = shift;
    }
    return $self->{ciphersuitess};
}
sub serverflags
{
    my $self = shift;
    if (@_) {
        $self->{serverflags} = shift;
    }
    return $self->{serverflags};
}
sub clientflags
{
    my $self = shift;
    if (@_) {
        $self->{clientflags} = shift;
    }
    return $self->{clientflags};
}
sub serverconnects
{
    my $self = shift;
    if (@_) {
        $self->{serverconnects} = shift;
    }
    return $self->{serverconnects};
}
# This is a bit ugly because the caller is responsible for keeping the records
# in sync with the updated message list; simply updating the message list isn't
# sufficient to get the proxy to forward the new message.
# But it does the trick for the one test (test_sslsessiontick) that needs it.
sub message_list
{
    my $self = shift;
    if (@_) {
        $self->{message_list} = shift;
    }
    return $self->{message_list};
}

sub fill_known_data
{
    my $length = shift;
    my $ret = "";
    for (my $i = 0; $i < $length; $i++) {
        $ret .= chr($i);
    }
    return $ret;
}

sub is_tls13
{
    my $class = shift;
    if (@_) {
        $is_tls13 = shift;
    }
    return $is_tls13;
}

sub reneg
{
    my $self = shift;
    if (@_) {
        $self->{reneg} = shift;
    }
    return $self->{reneg};
}

#Setting a sessionfile means that the client will not close until the given
#file exists. This is useful in TLSv1.3 where otherwise s_client will close
#immediately at the end of the handshake, but before the session has been
#received from the server. A side effect of this is that s_client never sends
#a close_notify, so instead we consider success to be when it sends application
#data over the connection.
sub sessionfile
{
    my $self = shift;
    if (@_) {
        $self->{sessionfile} = shift;
        TLSProxy::Message->successondata(1);
    }
    return $self->{sessionfile};
}
sub expected_tickets
{
    my $self = shift;
    if (@_) {
        $self->{expected_tickets} = shift;
    }
    return $self->{expected_tickets};
}

sub ciphersuite
{
    my $class = shift;
    if (@_) {
        $ciphersuite = shift;
    }
    return $ciphersuite;
}

sub isdtls
{
    my $self = shift;
    return $self->{isdtls}; #read-only
}

1;
