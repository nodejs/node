#! /usr/bin/env perl
# Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use File::Spec::Functions;
use File::Basename;
#use File::Copy;
#use File::Path;
use lib catdir(dirname($0), "perl");
use OpenSSL::Util::Pod;

my %dups;

sub parsenum()
{
    my $file = shift;
    my @apis;

    open my $IN, '<', $file
        or die "Can't open $file, $!, stopped";

    while ( <$IN> ) {
        next if /\sNOEXIST/;
        next if /EXPORT_VAR_AS_FUNC/;
        push @apis, $1 if /([^\s]+).\s/;
    }

    close $IN;

    print "# Found ", scalar(@apis), " in $file\n";
    return sort @apis;
}

sub getdocced()
{
    my $dir = shift;
    my %return;

    foreach my $pod ( glob("$dir/*.pod") ) {
        next if $pod eq 'doc/crypto/crypto.pod';
        next if $pod eq 'doc/ssl/ssl.pod';
        my %podinfo = extract_pod_info($pod);
        foreach my $n ( @{$podinfo{names}} ) {
            $return{$n} = $pod;
            print "# Duplicate $n in $pod and $dups{$n}\n"
                if defined $dups{$n};
            $dups{$n} = $pod;
        }
    }

    return %return;
}

sub printem()
{
    my $docdir = shift;
    my $numfile = shift;
    my %docced = &getdocced($docdir);
    my $count = 0;

    foreach my $func ( &parsenum($numfile) ) {
        next if $docced{$func};

        # Skip ASN1 utilities
        next if $func =~ /^ASN1_/;

        print $func, "\n";
        $count++;
    }
    print "# Found $count missing from $numfile\n\n";
}


&printem('doc/crypto', 'util/libcrypto.num');
&printem('doc/ssl', 'util/libssl.num');
