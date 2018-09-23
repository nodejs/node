# Copyright 2016-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;

use TLSProxy::Proxy;

package TLSProxy::Record;

my $server_ccs_seen = 0;
my $client_ccs_seen = 0;
my $etm = 0;

use constant TLS_RECORD_HEADER_LENGTH => 5;

#Record types
use constant {
    RT_APPLICATION_DATA => 23,
    RT_HANDSHAKE => 22,
    RT_ALERT => 21,
    RT_CCS => 20,
    RT_UNKNOWN => 100
};

my %record_type = (
    RT_APPLICATION_DATA, "APPLICATION DATA",
    RT_HANDSHAKE, "HANDSHAKE",
    RT_ALERT, "ALERT",
    RT_CCS, "CCS",
    RT_UNKNOWN, "UNKNOWN"
);

use constant {
    VERS_TLS_1_3 => 772,
    VERS_TLS_1_2 => 771,
    VERS_TLS_1_1 => 770,
    VERS_TLS_1_0 => 769,
    VERS_SSL_3_0 => 768,
    VERS_SSL_LT_3_0 => 767
};

my %tls_version = (
    VERS_TLS_1_3, "TLS1.3",
    VERS_TLS_1_2, "TLS1.2",
    VERS_TLS_1_1, "TLS1.1",
    VERS_TLS_1_0, "TLS1.0",
    VERS_SSL_3_0, "SSL3",
    VERS_SSL_LT_3_0, "SSL<3"
);

#Class method to extract records from a packet of data
sub get_records
{
    my $class = shift;
    my $server = shift;
    my $flight = shift;
    my $packet = shift;
    my $partial = "";
    my @record_list = ();
    my @message_list = ();
    my $data;
    my $content_type;
    my $version;
    my $len;
    my $len_real;
    my $decrypt_len;

    my $recnum = 1;
    while (length ($packet) > 0) {
        print " Record $recnum";
        if ($server) {
            print " (server -> client)\n";
        } else {
            print " (client -> server)\n";
        }
        #Get the record header
        if (length($packet) < TLS_RECORD_HEADER_LENGTH
                || length($packet) < 5 + unpack("n", substr($packet, 3, 2))) {
            print "Partial data : ".length($packet)." bytes\n";
            $partial = $packet;
            $packet = "";
        } else {
            ($content_type, $version, $len) = unpack('CnnC*', $packet);
            $data = substr($packet, 5, $len);

            print "  Content type: ".$record_type{$content_type}."\n";
            print "  Version: $tls_version{$version}\n";
            print "  Length: $len";
            if ($len == length($data)) {
                print "\n";
                $decrypt_len = $len_real = $len;
            } else {
                print " (expected), ".length($data)." (actual)\n";
                $decrypt_len = $len_real = length($data);
            }

            my $record = TLSProxy::Record->new(
                $flight,
                $content_type,
                $version,
                $len,
                0,
                $len_real,
                $decrypt_len,
                substr($packet, TLS_RECORD_HEADER_LENGTH, $len_real),
                substr($packet, TLS_RECORD_HEADER_LENGTH, $len_real)
            );

            if (($server && $server_ccs_seen)
                     || (!$server && $client_ccs_seen)) {
                if ($etm) {
                    $record->decryptETM();
                } else {
                    $record->decrypt();
                }
            }

            push @record_list, $record;

            #Now figure out what messages are contained within this record
            my @messages = TLSProxy::Message->get_messages($server, $record);
            push @message_list, @messages;

            $packet = substr($packet, TLS_RECORD_HEADER_LENGTH + $len_real);
            $recnum++;
        }
    }

    return (\@record_list, \@message_list, $partial);
}

sub clear
{
    $server_ccs_seen = 0;
    $client_ccs_seen = 0;
}

#Class level accessors
sub server_ccs_seen
{
    my $class = shift;
    if (@_) {
      $server_ccs_seen = shift;
    }
    return $server_ccs_seen;
}
sub client_ccs_seen
{
    my $class = shift;
    if (@_) {
      $client_ccs_seen = shift;
    }
    return $client_ccs_seen;
}
#Enable/Disable Encrypt-then-MAC
sub etm
{
    my $class = shift;
    if (@_) {
      $etm = shift;
    }
    return $etm;
}

sub new
{
    my $class = shift;
    my ($flight,
        $content_type,
        $version,
        $len,
        $sslv2,
        $len_real,
        $decrypt_len,
        $data,
        $decrypt_data) = @_;
    
    my $self = {
        flight => $flight,
        content_type => $content_type,
        version => $version,
        len => $len,
        sslv2 => $sslv2,
        len_real => $len_real,
        decrypt_len => $decrypt_len,
        data => $data,
        decrypt_data => $decrypt_data,
        orig_decrypt_data => $decrypt_data,
        sent => 0
    };

    return bless $self, $class;
}

#Decrypt using encrypt-then-MAC
sub decryptETM
{
    my ($self) = shift;

    my $data = $self->data;

    if($self->version >= VERS_TLS_1_1()) {
        #TLS1.1+ has an explicit IV. Throw it away
        $data = substr($data, 16);
    }

    #Throw away the MAC (assumes MAC is 20 bytes for now. FIXME)
    $data = substr($data, 0, length($data) - 20);

    #Find out what the padding byte is
    my $padval = unpack("C", substr($data, length($data) - 1));

    #Throw away the padding
    $data = substr($data, 0, length($data) - ($padval + 1));

    $self->decrypt_data($data);
    $self->decrypt_len(length($data));

    return $data;
}

#Standard decrypt
sub decrypt()
{
    my ($self) = shift;

    my $data = $self->data;

    if($self->version >= VERS_TLS_1_1()) {
        #TLS1.1+ has an explicit IV. Throw it away
        $data = substr($data, 16);
    }

    #Find out what the padding byte is
    my $padval = unpack("C", substr($data, length($data) - 1));

    #Throw away the padding
    $data = substr($data, 0, length($data) - ($padval + 1));

    #Throw away the MAC (assumes MAC is 20 bytes for now. FIXME)
    $data = substr($data, 0, length($data) - 20);

    $self->decrypt_data($data);
    $self->decrypt_len(length($data));

    return $data;
}

#Reconstruct the on-the-wire record representation
sub reconstruct_record
{
    my $self = shift;
    my $data;

    if ($self->{sent}) {
        return "";
    }
    $self->{sent} = 1;

    if ($self->sslv2) {
        $data = pack('n', $self->len | 0x8000);
    } else {
        $data = pack('Cnn', $self->content_type, $self->version, $self->len);
    }
    $data .= $self->data;

    return $data;
}

#Read only accessors
sub flight
{
    my $self = shift;
    return $self->{flight};
}
sub content_type
{
    my $self = shift;
    return $self->{content_type};
}
sub version
{
    my $self = shift;
    return $self->{version};
}
sub sslv2
{
    my $self = shift;
    return $self->{sslv2};
}
sub len_real
{
    my $self = shift;
    return $self->{len_real};
}
sub orig_decrypt_data
{
    my $self = shift;
    return $self->{orig_decrypt_data};
}

#Read/write accessors
sub decrypt_len
{
    my $self = shift;
    if (@_) {
      $self->{decrypt_len} = shift;
    }
    return $self->{decrypt_len};
}
sub data
{
    my $self = shift;
    if (@_) {
      $self->{data} = shift;
    }
    return $self->{data};
}
sub decrypt_data
{
    my $self = shift;
    if (@_) {
      $self->{decrypt_data} = shift;
    }
    return $self->{decrypt_data};
}
sub len
{
    my $self = shift;
    if (@_) {
      $self->{len} = shift;
    }
    return $self->{len};
}
1;
