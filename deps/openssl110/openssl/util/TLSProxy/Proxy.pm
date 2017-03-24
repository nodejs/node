# Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
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
use TLSProxy::ServerKeyExchange;
use TLSProxy::NewSessionTicket;

my $have_IPv6 = 0;
my $IP_factory;

sub new
{
    my $class = shift;
    my ($filter,
        $execute,
        $cert,
        $debug) = @_;

    my $self = {
        #Public read/write
        proxy_addr => "localhost",
        proxy_port => 4453,
        server_addr => "localhost",
        server_port => 4443,
        filter => $filter,
        serverflags => "",
        clientflags => "",
        serverconnects => 1,
        serverpid => 0,
        reneg => 0,

        #Public read
        execute => $execute,
        cert => $cert,
        debug => $debug,
        cipherc => "",
        ciphers => "AES128-SHA",
        flight => 0,
        record_list => [],
        message_list => [],
    };

    # IO::Socket::IP is on the core module list, IO::Socket::INET6 isn't.
    # However, IO::Socket::INET6 is older and is said to be more widely
    # deployed for the moment, and may have less bugs, so we try the latter
    # first, then fall back on the code modules.  Worst case scenario, we
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
        $IP_factory = sub { IO::Socket::INET6->new(@_); };
        $have_IPv6 = 1;
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
        } else {
            $IP_factory = sub { IO::Socket::INET->new(@_); };
        }
    }

    return bless $self, $class;
}

sub clearClient
{
    my $self = shift;

    $self->{cipherc} = "";
    $self->{flight} = 0;
    $self->{record_list} = [];
    $self->{message_list} = [];
    $self->{clientflags} = "";

    TLSProxy::Message->clear();
    TLSProxy::Record->clear();
}

sub clear
{
    my $self = shift;

    $self->clearClient;
    $self->{ciphers} = "AES128-SHA";
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

sub start
{
    my ($self) = shift;
    my $pid;

    $pid = fork();
    if ($pid == 0) {
        if (!$self->debug) {
            open(STDOUT, ">", File::Spec->devnull())
                or die "Failed to redirect stdout: $!";
            open(STDERR, ">&STDOUT");
        }
        my $execcmd = $self->execute
            ." s_server -no_comp -rev -engine ossltest -accept "
            .($self->server_port)
            ." -cert ".$self->cert." -naccept ".$self->serverconnects;
        if ($self->ciphers ne "") {
            $execcmd .= " -cipher ".$self->ciphers;
        }
        if ($self->serverflags ne "") {
            $execcmd .= " ".$self->serverflags;
        }
        exec($execcmd);
    }
    $self->serverpid($pid);

    return $self->clientstart;
}

sub clientstart
{
    my ($self) = shift;
    my $oldstdout;

    if(!$self->debug) {
        open DEVNULL, ">", File::Spec->devnull();
        $oldstdout = select(DEVNULL);
    }

    # Create the Proxy socket
    my $proxaddr = $self->proxy_addr;
    $proxaddr =~ s/[\[\]]//g; # Remove [ and ]
    my $proxy_sock = $IP_factory->(
        LocalHost   => $proxaddr,
        LocalPort   => $self->proxy_port,
        Proto       => "tcp",
        Listen      => SOMAXCONN,
        ReuseAddr   => 1
    );

    if ($proxy_sock) {
        print "Proxy started on port ".$self->proxy_port."\n";
    } else {
        warn "Failed creating proxy socket (".$proxaddr.",".$self->proxy_port."): $!\n";
        return 0;
    }

    if ($self->execute) {
        my $pid = fork();
        if ($pid == 0) {
            if (!$self->debug) {
                open(STDOUT, ">", File::Spec->devnull())
                    or die "Failed to redirect stdout: $!";
                open(STDERR, ">&STDOUT");
            }
            my $echostr;
            if ($self->reneg()) {
                $echostr = "R";
            } else {
                $echostr = "test";
            }
            my $execcmd = "echo ".$echostr." | ".$self->execute
                 ." s_client -engine ossltest -connect "
                 .($self->proxy_addr).":".($self->proxy_port);
            if ($self->cipherc ne "") {
                $execcmd .= " -cipher ".$self->cipherc;
            }
            if ($self->clientflags ne "") {
                $execcmd .= " ".$self->clientflags;
            }
            exec($execcmd);
        }
    }

    # Wait for incoming connection from client
    my $client_sock;
    if(!($client_sock = $proxy_sock->accept())) {
        warn "Failed accepting incoming connection: $!\n";
        return 0;
    }

    print "Connection opened\n";

    # Now connect to the server
    my $retry = 3;
    my $server_sock;
    #We loop over this a few times because sometimes s_server can take a while
    #to start up
    do {
        my $servaddr = $self->server_addr;
        $servaddr =~ s/[\[\]]//g; # Remove [ and ]
        eval {
            $server_sock = $IP_factory->(
                PeerAddr => $servaddr,
                PeerPort => $self->server_port,
                MultiHomed => 1,
                Proto => 'tcp'
            );
        };

        $retry--;
        #Some buggy IP factories can return a defined server_sock that hasn't
        #actually connected, so we check peerport too
        if ($@ || !defined($server_sock) || !defined($server_sock->peerport)) {
            $server_sock->close() if defined($server_sock);
            undef $server_sock;
            if ($retry) {
                #Sleep for a short while
                select(undef, undef, undef, 0.1);
            } else {
                warn "Failed to start up server (".$servaddr.",".$self->server_port."): $!\n";
                return 0;
            }
        }
    } while (!$server_sock);

    my $sel = IO::Select->new($server_sock, $client_sock);
    my $indata;
    my @handles = ($server_sock, $client_sock);

    #Wait for either the server socket or the client socket to become readable
    my @ready;
    while(!(TLSProxy::Message->end) && (@ready = $sel->can_read)) {
        foreach my $hand (@ready) {
            if ($hand == $server_sock) {
                $server_sock->sysread($indata, 16384) or goto END;
                $indata = $self->process_packet(1, $indata);
                $client_sock->syswrite($indata);
            } elsif ($hand == $client_sock) {
                $client_sock->sysread($indata, 16384) or goto END;
                $indata = $self->process_packet(0, $indata);
                $server_sock->syswrite($indata);
            } else {
                print "Err\n";
                goto END;
            }
        }
    }

    END:
    print "Connection closed\n";
    if($server_sock) {
        $server_sock->close();
    }
    if($client_sock) {
        #Closing this also kills the child process
        $client_sock->close();
    }
    if($proxy_sock) {
        $proxy_sock->close();
    }
    if(!$self->debug) {
        select($oldstdout);
    }
    $self->serverconnects($self->serverconnects - 1);
    if ($self->serverconnects == 0) {
        die "serverpid is zero\n" if $self->serverpid == 0;
        print "Waiting for server process to close: "
              .$self->serverpid."\n";
        waitpid( $self->serverpid, 0);
        die "exit code $? from server process\n" if $? != 0;
    }
    return 1;
}

sub process_packet
{
    my ($self, $server, $packet) = @_;
    my $len_real;
    my $decrypt_len;
    my $data;
    my $recnum;

    if ($server) {
        print "Received server packet\n";
    } else {
        print "Received client packet\n";
    }

    print "Packet length = ".length($packet)."\n";
    print "Processing flight ".$self->flight."\n";

    #Return contains the list of record found in the packet followed by the
    #list of messages in those records
    my @ret = TLSProxy::Record->get_records($server, $self->flight, $packet);
    push @{$self->record_list}, @{$ret[0]};
    push @{$self->{message_list}}, @{$ret[1]};

    print "\n";

    #Finished parsing. Call user provided filter here
    if(defined $self->filter) {
        $self->filter->($self);
    }

    #Reconstruct the packet
    $packet = "";
    foreach my $record (@{$self->record_list}) {
        #We only replay the records for the current flight
        if ($record->flight != $self->flight) {
            next;
        }
        $packet .= $record->reconstruct_record();
    }

    $self->{flight} = $self->{flight} + 1;

    print "Forwarded packet length = ".length($packet)."\n\n";

    return $packet;
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

#Read/write accessors
sub proxy_addr
{
    my $self = shift;
    if (@_) {
      $self->{proxy_addr} = shift;
    }
    return $self->{proxy_addr};
}
sub proxy_port
{
    my $self = shift;
    if (@_) {
      $self->{proxy_port} = shift;
    }
    return $self->{proxy_port};
}
sub server_addr
{
    my $self = shift;
    if (@_) {
      $self->{server_addr} = shift;
    }
    return $self->{server_addr};
}
sub server_port
{
    my $self = shift;
    if (@_) {
      $self->{server_port} = shift;
    }
    return $self->{server_port};
}
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
sub ciphers
{
    my $self = shift;
    if (@_) {
      $self->{ciphers} = shift;
    }
    return $self->{ciphers};
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
sub serverpid
{
    my $self = shift;
    if (@_) {
      $self->{serverpid} = shift;
    }
    return $self->{serverpid};
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

sub reneg
{
    my $self = shift;
    if (@_) {
      $self->{reneg} = shift;
    }
    return $self->{reneg};
}

1;
