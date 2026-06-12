#! /usr/bin/env perl
# Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test qw/:DEFAULT srctop_dir srctop_file result_dir/;
use OpenSSL::Test::Utils;
use File::Temp qw(tempfile);
use File::Path 2.00 qw(rmtree mkpath);

setup("test_memfail");

#
# Don't run this test if allocfail-tests isn't enabled, it won't work
#
plan skip_all => "$test_name requires allocfail-tests to be enabled"
    if disabled("allocfail-tests");

#
# We need to know how many mallocs we plan to fail, so run the test in count mode
# To tell us how many mallocs it executes
# We capture the result of the test into countinfo.txt
# and parse that to figure out what our values are
#
my $resultdir = result_dir();
run(test(["handshake-memfail", "count", srctop_dir("test", "certs")], stderr => "$resultdir/hscountinfo.txt"));

run(test(["x509-memfail", "count", srctop_file("test", "certs", "servercert.pem")], stderr => "$resultdir/x509countinfo.txt"));

sub get_count_info {
    my ($infile) = @_;
    my @vals;

    # Read in our input file
    open my $handle, '<', "$infile";
    chomp(my @lines = <$handle>);
    close $handle;

    # parse the input file
    foreach(@lines) {
        if ($_ =~/skip:/) {
            @vals = split ' ', $_;
            last;
        }
    }
    #
    #The number of allocations we skip is in argument 2
    #The number of mallocs we shoudl test is in argument 4
    #
    my $skipcount = $vals[2];
    my $malloccount = $vals[4];
    return ($skipcount, $malloccount);
}

my ($hsskipcount, $hsmalloccount) = get_count_info("$resultdir/hscountinfo.txt");

my ($x509skipcount, $x509malloccount) = get_count_info("$resultdir/x509countinfo.txt");

#
# Now we can plan our tests.  We plan to run malloccount iterations of this
# test
#
plan tests => $hsmalloccount + $x509malloccount;

sub run_memfail_test {
    my $skipcount = $_[0];
    my @mallocseq = (1..$_[1]);
    my @cmd = $_[2];

    for my $idx (@mallocseq) {
        #
        # We need to setup our openssl malloc failures env var to fail the target malloc
        # the format of this string is a series of A@B;C@D tuples where A,C are the number
        # of mallocs to consider, and B,D are the likelyhood that they should fail.
        # We always skip the first "skip" allocations, then iteratively guarantee that
        # next <idx> mallocs pass, followed by the next single malloc failing, with the remainder
        # passing
        #
        $ENV{OPENSSL_MALLOC_FAILURES} = "$skipcount\@0;$idx\@0;1\@100;0\@0";
        ok(run(test(@cmd)));
    }
}

run_memfail_test($hsskipcount, $hsmalloccount, ["handshake-memfail", "run", srctop_dir("test", "certs")]);

run_memfail_test($x509skipcount, $x509malloccount, ["x509-memfail", "run", srctop_file("test", "certs", "servercert.pem")]);

