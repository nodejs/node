# Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;

package TLSProxy::ServerKeyExchange;

use vars '@ISA';
push @ISA, 'TLSProxy::Message';

sub new
{
    my $class = shift;
    my ($server,
        $data,
        $records,
        $startoffset,
        $message_frag_lens) = @_;
    
    my $self = $class->SUPER::new(
        $server,
        TLSProxy::Message::MT_SERVER_KEY_EXCHANGE,
        $data,
        $records,
        $startoffset,
        $message_frag_lens);

    #DHE
    $self->{p} = "";
    $self->{g} = "";
    $self->{pub_key} = "";
    $self->{sig} = "";

    return $self;
}

sub parse
{
    my $self = shift;

    #Minimal SKE parsing. Only supports DHE at the moment (if its not DHE
    #the parsing data will be trash...which is ok as long as we don't try to
    #use it)

    my $p_len = unpack('n', $self->data);
    my $ptr = 2;
    my $p = substr($self->data, $ptr, $p_len);
    $ptr += $p_len;

    my $g_len = unpack('n', substr($self->data, $ptr));
    $ptr += 2;
    my $g = substr($self->data, $ptr, $g_len);
    $ptr += $g_len;

    my $pub_key_len = unpack('n', substr($self->data, $ptr));
    $ptr += 2;
    my $pub_key = substr($self->data, $ptr, $pub_key_len);
    $ptr += $pub_key_len;

    #We assume its signed
    my $sig_len = unpack('n', substr($self->data, $ptr));
    my $sig = "";
    if (defined $sig_len) {
	$ptr += 2;
	$sig = substr($self->data, $ptr, $sig_len);
	$ptr += $sig_len;
    }

    $self->p($p);
    $self->g($g);
    $self->pub_key($pub_key);
    $self->sig($sig);
}


#Reconstruct the on-the-wire message data following changes
sub set_message_contents
{
    my $self = shift;
    my $data;

    $data = pack('n', length($self->p));
    $data .= $self->p;
    $data .= pack('n', length($self->g));
    $data .= $self->g;
    $data .= pack('n', length($self->pub_key));
    $data .= $self->pub_key;
    if (length($self->sig) > 0) {
        $data .= pack('n', length($self->sig));
        $data .= $self->sig;
    }

    $self->data($data);
}

#Read/write accessors
#DHE
sub p
{
    my $self = shift;
    if (@_) {
      $self->{p} = shift;
    }
    return $self->{p};
}
sub g
{
    my $self = shift;
    if (@_) {
      $self->{g} = shift;
    }
    return $self->{g};
}
sub pub_key
{
    my $self = shift;
    if (@_) {
      $self->{pub_key} = shift;
    }
    return $self->{pub_key};
}
sub sig
{
    my $self = shift;
    if (@_) {
      $self->{sig} = shift;
    }
    return $self->{sig};
}
1;
