#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use Math::BigInt;

use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_bn");

plan tests => 3;

require_ok(srctop_file("test","recipes","bc.pl"));

my $testresults = "tmp.bntest";
my $init = ok(run(test(["bntest"], stdout => $testresults)), 'initialize');

 SKIP: {
     skip "Initializing failed, skipping", 1 if !$init;

     subtest 'Checking the bn results' => sub {
	 my @lines = ();
	 if (open DATA, $testresults) {
	     @lines = <DATA>;
	     close DATA;
	 }
	 map { s/\R//; } @lines;	# chomp(@lines);

	 plan tests => scalar grep(/^print /, @lines);

	 my $l = "";

	 while (scalar @lines) {
	     $l = shift @lines;

	     last if $l =~ /^print /;
	 }

	 while (1) {
	     $l =~ s/^print "//;
	     $l =~ s/\\n"//;
	     my $t = $l;
	     my @operations = ();

	     $l = undef;
	     while (scalar @lines) {
		 $l = shift @lines;

		 last if $l =~ /^print /;
		 push @operations, $l;
		 $l = undef;
	     }

	     ok(check_operations(@operations), "verify $t");

	     last unless $l;
	 }
     };
 }

unlink $testresults;

sub check_operations {
    my $failcount = 0;

    foreach my $line (@_) {
	my $result = calc(split /\s+/, $line);

	if ($result ne "0" && $result ne "0x0") {
	    $failcount++;
	    print STDERR "Failed! $line => $result\n";
	}
    }

    return $failcount == 0;
}
