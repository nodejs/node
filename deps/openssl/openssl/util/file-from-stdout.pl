#! /usr/bin/env perl
# Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

# Run a command and make its standard output into a file, in place of the
# shell's ">".  The output is collected in a temporary file and renamed over
# the target once the command has succeeded, so a failing command leaves the
# previous target rather than a truncated one with a fresh timestamp.
#
# The target is left read-only.  Renaming onto a read-only file needs no
# permission on the file itself, so it can still be replaced.
#
# Usage: file-from-stdout.pl [-x] FILE command [args...]

my $executable = 0;

if (@ARGV && $ARGV[0] eq "-x") {
    $executable = 1;
    shift @ARGV;
}

my $target = shift @ARGV;

die "Usage: $0 [-x] FILE command [args...]\n"
    unless defined $target && @ARGV;

my $temp = "$target.tmp$$";

unlink $temp;

open my $out, ">", $temp
    or die "Can't write $temp, $!\n";

END {
    unlink $temp if defined $temp && -e $temp;
}

my $exec = shift @ARGV;
open my $saved, ">&", \*STDOUT
    or die "Can't save stdout, $!\n";
open STDOUT, ">&", $out
    or die "Can't redirect stdout to $temp, $!\n";

my $status = system($exec, @ARGV);
my $why = $!;

open STDOUT, ">&", $saved
    or die "Can't restore stdout, $!\n";
close $saved;
close $out
    or die "Can't finish writing $temp, $!\n";

if ($status != 0) {
    my $how = $status == -1 ? "could not be run, $why"
            : $status & 127 ? "died with signal " . ($status & 127)
            :                 "exited with " . ($status >> 8);
    my $code = $status == -1 || ($status & 127) ? 1 : $status >> 8;

    print STDERR "$exec $how\n";
    exit $code;
}

chmod $executable ? 0555 : 0444, $temp
    or die "Can't set the mode of $temp, $!\n";

rename $temp, $target
    or die "Can't rename $temp to $target, $!\n";
