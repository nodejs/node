#! /usr/bin/env perl
# Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

require 5.10.0;
use warnings;
use strict;

use File::Basename;
use File::Compare;
use File::Copy;

# Install a set of files into one directory in a single process,
# replacing only the files whose content changed; an installed file
# that is already identical keeps its timestamp, so consumers that
# build against the installed tree do not see it as modified.

if ($#ARGV + 1 < 2) {
    print STDERR "Usage: install-files.pl mode target-dir [file...]\n";
    exit 1;
}

my ($mode, $targetdir, @files) = @ARGV;

$mode = oct($mode);

foreach my $file (@files) {
    my $target = "$targetdir/" . basename($file);

    next if -f $target && compare($file, $target) == 0;
    print "install $file -> $target\n";
    copy($file, $target)
        || die "Can't install $file to $target, $!\n";
    chmod $mode, $target
        || die "Can't chmod $target, $!\n";
}
