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
use FindBin;
use lib "$FindBin::Bin/perl";

use OpenSSL::Util::Pod;

# Install or uninstall a whole man section's pages and their NAME
# symlinks in a single process; one process per page does not scale
# to the number of pages OpenSSL installs.

if ($#ARGV + 1 < 5 || $ARGV[0] !~ /^(un)?install$/) {
    print STDERR
        "Usage: install-man-pages.pl [install|uninstall] src-dir build-dir target-dir man-suffix [man-page-file...]\n";
    exit 1;
}

my ($action, $srcdir, $builddir, $targetdir, $suffix, @files) = @ARGV;

foreach my $file (@files) {
    my $manname = basename($file);

    $manname =~ m|(.+)\.(.+)|;
    my $mainf = $1;
    my $section = $2;
    die "Bad man page name $file\n" if !defined $mainf;

    my $target = "$targetdir/$manname$suffix";

    if ($action eq "install") {
        # A page is a regular file; a symlink here is a leftover alias,
        # and copying would write this page over the one it points to.
        unlink $target if -l $target;

        next if -f $target && compare($file, $target) == 0;

        print "install $file -> $target\n";
        copy($file, $target)
            || die "Can't install $file to $target, $!\n";
        chmod 0644, $target
            || die "Can't chmod $target, $!\n";
    } else {
        print "rm -f $target\n";
        unlink $target;
    }

    my $podfile = "$srcdir/$mainf.pod";
    # Some pod files are generated and are in the build dir
    unless (-e $podfile) {
        $podfile = "$builddir/$mainf.pod";
    }
    my %podinfo = extract_pod_info($podfile);

    for my $name (@{$podinfo{names}}) {
        next if $name eq $mainf;
        my $link = "$targetdir/$name.$section$suffix";
        my $linktarget = "$manname$suffix";

        next if $action eq "install"
            && -l $link && readlink($link) eq $linktarget;

        # An alias can collide with an installed page on a
        # case-insensitive file system (OSSL_TRACE_ENABLED alongside
        # OSSL_trace_enabled), where unlinking the alias would remove
        # the page and the symlink would point to itself.  Never
        # remove anything that is not a symlink, and skip an alias
        # whose name is already occupied.
        unlink $link if -l $link;
        if ($action eq "install") {
            symlink $linktarget, $link
                || warn "Can't symlink $link to $linktarget, $!\n"
                unless -e $link;
        }
    }
}
