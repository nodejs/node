#! /usr/bin/env perl
# Copyright 2021-2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;
use File::Spec;

package OpenSSL::copyright;

sub year_of {
    my $file = shift;

    # A generated file is staged as FILE.new before it is compared to,
    # and possibly replaces, FILE; the year that belongs to it is the
    # year of the file it is staged to replace.
    $file =~ s/\.new$//;

    return $ENV{'OSSL_COPYRIGHT_YEAR'} if defined $ENV{'OSSL_COPYRIGHT_YEAR'};

    # Get the current year.  We use that as the default because the other
    # common case is that someone unpacked a tarfile and the file dates
    # are't properly set on extract.
    my $YEAR = [localtime()]->[5] + 1900;

    # See if git's available
    my $devnull = File::Spec->devnull();
    open my $FH,
       "git log -1 --date=short --format=format:%cd $file 2>$devnull|"
           or return $YEAR;
    my $LINE = <$FH> // '';
    close $FH;
    $LINE =~ s/^([0-9]*)-.*/$1/;
    $YEAR = $LINE if $LINE;
    return $YEAR;
}

sub latest {
    my $l = 0;
    foreach my $f (@_ ) {
        my $y = year_of($f);
        $l = $y if $y > $l;
    }
    return $l
}
1;
