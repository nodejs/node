#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Compare qw/compare_text/;
use File::Copy;
use OpenSSL::Test qw/:DEFAULT/;

my %conversionforms = (
    # Default conversion forms.  Other series may be added with
    # specific test types as key.
    "*"		=> [ "d", "p" ],
    "msb"	=> [ "d", "p", "msblob" ],
    );
sub tconversion {
    my $testtype = shift;
    my $t = shift;
    my @conversionforms =
	defined($conversionforms{$testtype}) ?
	@{$conversionforms{$testtype}} :
	@{$conversionforms{"*"}};
    my @openssl_args = @_;
    if (!@openssl_args) { @openssl_args = ($testtype); }

    my $n = scalar @conversionforms;
    my $totaltests =
	1			# for initializing
	+ $n			# initial conversions from p to all forms (A)
	+ $n*$n			# conversion from result of A to all forms (B)
	+ 1			# comparing original test file to p form of A
	+ $n*($n-1);		# comparing first conversion to each fom in A with B
    $totaltests-- if ($testtype eq "p7d"); # no comparison of original test file
    plan tests => $totaltests;

    my @cmd = ("openssl", @openssl_args);

    my $init;
    if (scalar @openssl_args > 0 && $openssl_args[0] eq "pkey") {
	$init = ok(run(app([@cmd, "-in", $t, "-out", "$testtype-fff.p"])),
		   'initializing');
    } else {
	$init = ok(copy($t, "$testtype-fff.p"), 'initializing');
    }
    if (!$init) {
	diag("Trying to copy $t to $testtype-fff.p : $!");
    }

  SKIP: {
      skip "Not initialized, skipping...", 22 unless $init;

      foreach my $to (@conversionforms) {
	  ok(run(app([@cmd,
		      "-in", "$testtype-fff.p",
		      "-inform", "p",
		      "-out", "$testtype-f.$to",
		      "-outform", $to])),
	     "p -> $to");
      }

      foreach my $to (@conversionforms) {
	  foreach my $from (@conversionforms) {
	      ok(run(app([@cmd,
			  "-in", "$testtype-f.$from",
			  "-inform", $from,
			  "-out", "$testtype-ff.$from$to",
			  "-outform", $to])),
		 "$from -> $to");
	  }
      }

      if ($testtype ne "p7d") {
	  is(cmp_text("$testtype-fff.p", "$testtype-f.p"), 0,
	     'comparing orig to p');
      }

      foreach my $to (@conversionforms) {
	  next if $to eq "d";
	  foreach my $from (@conversionforms) {
	      is(cmp_text("$testtype-f.$to", "$testtype-ff.$from$to"), 0,
		 "comparing $to to $from$to");
	  }
      }
    }
    unlink glob "$testtype-f.*";
    unlink glob "$testtype-ff.*";
    unlink glob "$testtype-fff.*";
}

sub cmp_text {
    return compare_text(@_, sub {
        $_[0] =~ s/\R//g;
        $_[1] =~ s/\R//g;
        return $_[0] ne $_[1];
    });
}

1;
