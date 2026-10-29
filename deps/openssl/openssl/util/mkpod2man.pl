#! /usr/bin/env perl
# Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use Getopt::Std;
use File::Basename;
use Pod::Man;

# Format man pages without loading Pod::Man once per page.  pod2man is a
# perl program that compiles Pod::Man before it reads a single pod, and
# OpenSSL has over nine hundred pages, so running it once per page spends
# most of that time compiling the same module again and again.  Here the
# module is loaded once and the pages are then divided between a few
# forked workers, which inherit it already compiled.

# Options.  The whole manual is formatted in one run, so the section of
# each page is taken from the directory its pod lives in rather than
# given on the command line: doc/man3/BIO_s_mem.pod is section 3 and
# becomes OUTDIR/man3/BIO_s_mem.3.
our ($opt_o);    # -o OUTDIR, the directory holding the man1..man7 dirs
our ($opt_m);    # -m MANSUFFIX, appended to the section inside the page
our ($opt_d);    # -d DATE
our ($opt_r);    # -r RELEASE
our ($opt_j);    # -j JOBS

getopts('o:m:d:r:j:');
die "-o flag missing" unless defined $opt_o;
$opt_m = '' unless defined $opt_m;
$opt_d = '' unless defined $opt_d;
$opt_r = '' unless defined $opt_r;

sub cpu_count
{
    my $cpus = $ENV{"NUMBER_OF_PROCESSORS"};    # Windows sets this.

    if (!defined($cpus) && $^O =~ /linux/) {
        my $tmp = qx(nproc 2>/dev/null);

        $cpus = $tmp if $? == 0 && $tmp > 0;
    }
    if (!defined($cpus) && -r "/proc/cpuinfo") {
        my $tmp = qx(grep -c ^processor /proc/cpuinfo 2>/dev/null);

        $cpus = $tmp if $? == 0 && $tmp > 0;
    }
    if (!defined($cpus)) {
        my $tmp = qx(sysctl -n hw.ncpu 2>/dev/null);    # BSDs, macOS

        $cpus = $tmp if $? == 0 && $tmp > 0;
    }

    return defined($cpus) && $cpus > 0 ? int($cpus) : 1;
}

# Turn one pod into one man page.
sub format_page
{
    my $pod = shift;
    my $name = basename($pod, ".pod");
    my $dir = basename(dirname($pod));
    my ($section) = $dir =~ m|^man(\d)$|;

    die "Can't tell the section of $pod from its directory\n"
        unless defined $section;

    my $out = "$opt_o/man$section/$name.$section";

    # The page is current when it is newer than the pod it comes from.
    return if -e $out && -M $out < -M $pod;

    unlink $out;

    Pod::Man->new(name => uc $name,
                  section => "$section$opt_m",
                  center => "OpenSSL",
                  date => $opt_d,
                  release => $opt_r)
        ->parse_from_file($pod, $out);

    chmod 0444, $out
        or die "Can't set the mode of $out, $!\n";
}

my @pods = @ARGV;

exit 0 unless @pods;

# Only ask how many processors there are when the answer can matter;
# the count is found by running a command, and the per-page rules call
# this script with a single pod.
my $jobs = @pods > 1
    ? (defined $opt_j && $opt_j > 0 ? int($opt_j) : cpu_count())
    : 1;

$jobs = scalar @pods if $jobs > @pods;

# One page, or no reason to fork: do the work here.  This is also the path
# taken where fork() is emulated and would cost more than it saves.
if ($jobs <= 1) {
    format_page($_) foreach @pods;
    exit 0;
}

my @pids;

foreach my $worker (0 .. $jobs - 1) {
    my $pid = fork();

    die "Can't fork, $!\n" unless defined $pid;
    if (!$pid) {
        # Deal every $jobs'th page to this worker.  The pages differ a lot
        # in size, and dealing them out interleaves the large ones instead
        # of handing one worker a contiguous run of them.
        for (my $i = $worker; $i <= $#pods; $i += $jobs) {
            format_page($pods[$i]);
        }
        exit 0;
    }
    push @pids, $pid;
}

my $failed = 0;

foreach my $pid (@pids) {
    waitpid($pid, 0);
    $failed = 1 if $?;
}

die "Failed to format the manual pages\n" if $failed;
