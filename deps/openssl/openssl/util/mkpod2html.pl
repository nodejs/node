#! /usr/bin/env perl
# Copyright 2020-2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use lib ".";
use Getopt::Std;
use File::Basename;
use Pod::Html;
use File::Spec::Functions qw(:DEFAULT rel2abs);

# With -i, convert the one named pod, which is what the per-page rules and
# the other build systems do.  Without it, convert every pod named on the
# command line, deriving each output from the pod's own path.  Pod::Html is
# expensive to compile and OpenSSL has over nine hundred pages, so loading
# it once and dividing the pages between a few forked workers is far
# cheaper than starting this program once per page.

# Options.
our ($opt_i);    # -i INFILE, one page; without it, pods are read from @ARGV
our ($opt_o);    # -o OUTFILE with -i, otherwise the directory to write into
our ($opt_t);    # -t TITLE, only with -i
our ($opt_r);    # -r PODROOT
our ($opt_j);    # -j JOBS, only without -i

getopts('i:o:t:r:j:');
die "-o flag missing" unless $opt_o;
die "-r flag missing" unless $opt_r;

# We originally used realpath() here, but the Windows implementation appears
# to require that the directory or file exist to be able to process the input,
# so we use rel2abs() instead, which only processes the string without
# looking further.
my $podroot = rel2abs($opt_r) or die "Can't convert to real path: $!";

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

# Turn one pod into one HTML page.
sub format_page
{
    my ($pod, $out, $title) = @_;

    $pod = rel2abs($pod) or die "Can't convert to real path: $!";
    $out = rel2abs($out) or die "Can't convert to real path: $!";

    unlink $out;

    pod2html "--infile=$pod",
             "--outfile=$out",
             "--title=$title",
             "--podroot=$podroot",
             "--podpath=man1:man3:man5:man7",
             "--htmldir=..";

    # Read in contents.
    open my $fh, "<", $out
        or die "Can't read $out, $!";
    my $contents = do { local $/ = undef; <$fh> };
    close $fh;
    unlink $out;

    $contents =~
        s|href="http://man\.he\.net/(man\d/[^"]+)(?:\.html)?"|href="../$1.html"|g;
    open $fh, ">", $out
        or die "Can't write $out, $!";
    print $fh $contents;
    close $fh;

    chmod 0444, $out
        or die "Can't set the mode of $out, $!\n";
}

# One named page: the output file and title are given.
if (defined $opt_i) {
    die "-t flag missing" unless $opt_t;
    format_page($opt_i, $opt_o, $opt_t);
    exit 0;
}

# Otherwise every pod named on the command line, with $opt_o the directory
# holding the man1..man7 subdirectories: doc/man3/BIO_s_mem.pod becomes
# $opt_o/man3/BIO_s_mem.html, titled BIO_s_mem.
sub page_of
{
    my $pod = shift;
    my $name = basename($pod, ".pod");
    my ($section) = basename(dirname($pod)) =~ m|^man(\d)$|;

    die "Can't tell the section of $pod from its directory\n"
        unless defined $section;

    return ("$opt_o/man$section/$name.html", $name);
}

my @pods = @ARGV;

exit 0 unless @pods;

# Only ask how many processors there are when the answer can matter; the
# count is found by running a command.
my $jobs = @pods > 1
    ? (defined $opt_j && $opt_j > 0 ? int($opt_j) : cpu_count())
    : 1;

$jobs = scalar @pods if $jobs > @pods;

sub format_pods
{
    foreach my $pod (@_) {
        my ($out, $title) = page_of($pod);

        # The page is current when it is newer than the pod it comes from.
        next if -e $out && -M $out < -M $pod;

        format_page($pod, $out, $title);
    }
}

if ($jobs <= 1) {
    format_pods(@pods);
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
        my @mine;

        for (my $i = $worker; $i <= $#pods; $i += $jobs) {
            push @mine, $pods[$i];
        }
        format_pods(@mine);
        exit 0;
    }
    push @pids, $pid;
}

my $failed = 0;

foreach my $pid (@pids) {
    waitpid($pid, 0);
    $failed = 1 if $?;
}

die "Failed to convert the manual pages to HTML\n" if $failed;
