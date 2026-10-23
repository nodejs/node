# Copyright 2024-2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;

package TLSProxy::RecordNumber;

sub new
{
    my $class = shift;
    my ($epoch,
        $seqnum) = @_;

    my $self = {
        epoch => $epoch,
        seqnum => $seqnum
    };

    return bless $self, $class;
}

# Read only accessors
sub epoch
{
    my $self = shift;
    return $self->{epoch};
}
sub seqnum
{
    my $self = shift;
    return $self->{seqnum};
}
1;
