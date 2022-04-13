#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_ordinals");

plan tests => 2;

ok(testordinals(srctop_file("util", "libcrypto.num")), "Test libcrypto.num");
ok(testordinals(srctop_file("util", "libssl.num")), "Test libssl.num");

sub testordinals
{
    my $filename = shift;
    my $cnt = 0;
    my $ret = 1;
    my $qualifier = "";
    my $newqual;
    my $lastfunc = "";

    open(my $fh, '<', $filename);
    while (my $line = <$fh>) {
        my @tokens = split(/(?:\s+|\s*:\s*)/, $line);
        #Check the line looks sane
        if ($#tokens < 5 || $#tokens > 6) {
            print STDERR "Invalid line:\n$line\n";
            $ret = 0;
            last;
        }
        if ($tokens[3] eq "NOEXIST") {
            #Ignore this line
            next;
        }
        #Some ordinals can be repeated, e.g. if one is VMS and another is !VMS
        $newqual = $tokens[4];
        $newqual =~ s/!//g;
        my $number = $tokens[1];
        $number = $cnt + 1 if $number eq '?';
        $number = $cnt if $number eq '?+';
        if ($cnt > $number
                || ($cnt == $number && ($qualifier ne $newqual
                                           || $qualifier eq ""))) {
            print STDERR "Invalid ordinal detected: ".$tokens[1]."\n";
            $ret = 0;
            last;
        }
        $cnt = $tokens[1];
        $qualifier = $newqual;
        $lastfunc = $tokens[0];
    }
    close($fh);

    return $ret;
}
