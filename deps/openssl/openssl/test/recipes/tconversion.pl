#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
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
    my %opts = @_;

    die "Missing option -type" unless $opts{-type};
    die "Missing option -in" unless $opts{-in};
    my $testtype = $opts{-type};
    my $t = $opts{-in};
    my $prefix = $opts{-prefix} // $testtype;
    my @conversionforms =
	defined($conversionforms{$testtype}) ?
	@{$conversionforms{$testtype}} :
	@{$conversionforms{"*"}};
    my @openssl_args;
    if (defined $opts{-args}) {
        @openssl_args = @{$opts{-args}} if ref $opts{-args} eq 'ARRAY';
        @openssl_args = ($opts{-args}) if ref $opts{-args} eq '';
    }
    @openssl_args = ($testtype) unless @openssl_args;

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
	$init = ok(run(app([@cmd, "-in", $t, "-out", "$prefix-fff.p"])),
		   'initializing');
    } else {
	$init = ok(copy($t, "$prefix-fff.p"), 'initializing');
    }
    if (!$init) {
	diag("Trying to copy $t to $prefix-fff.p : $!");
    }

  SKIP: {
      skip "Not initialized, skipping...", 22 unless $init;

      foreach my $to (@conversionforms) {
	  ok(run(app([@cmd,
		      "-in", "$prefix-fff.p",
		      "-inform", "p",
		      "-out", "$prefix-f.$to",
		      "-outform", $to])),
	     "p -> $to");
      }

      foreach my $to (@conversionforms) {
	  foreach my $from (@conversionforms) {
	      ok(run(app([@cmd,
			  "-in", "$prefix-f.$from",
			  "-inform", $from,
			  "-out", "$prefix-ff.$from$to",
			  "-outform", $to])),
		 "$from -> $to");
	  }
      }

      if ($testtype ne "p7d") {
	  is(cmp_text("$prefix-fff.p", "$prefix-f.p"), 0,
	     'comparing orig to p');
      }

      foreach my $to (@conversionforms) {
	  next if $to eq "d";
	  foreach my $from (@conversionforms) {
	      is(cmp_text("$prefix-f.$to", "$prefix-ff.$from$to"), 0,
		 "comparing $to to $from$to");
	  }
      }
    }
}

sub cmp_text {
    return compare_text(@_, sub {
        $_[0] =~ s/\R//g;
        $_[1] =~ s/\R//g;
        return $_[0] ne $_[1];
    });
}

sub file_contains {
    $_ = shift @_;
    my $pattern = shift @_;
    open(DATA, $_) or return 0;
    $_= join('', <DATA>);
    close(DATA);
    return m/$pattern/ ? 1 : 0;
}

sub cert_contains {
    my $cert = shift @_;
    my $pattern = shift @_;
    my $expected = shift @_;
    my $name = shift @_;
    my $out = "cert_contains.out";
    run(app(["openssl", "x509", "-noout", "-text", "-in", $cert, "-out", $out]));
    is(file_contains($out, $pattern), $expected, ($name ? "$name: " : "").
       "$cert should ".($expected ? "" : "not ")."contain $pattern");
    # not unlinking $out
}

sub uniq (@) {
    my %seen = ();
    grep { not $seen{$_}++ } @_;
}

sub file_n_different_lines {
    my $filename = shift @_;
    open(DATA, $filename) or return 0;
    chomp(my @lines = <DATA>);
    close(DATA);
    return scalar(uniq @lines);
}

sub cert_ext_has_n_different_lines {
    my $cert = shift @_;
    my $expected = shift @_;
    my $exts = shift @_;
    my $name = shift @_;
    my $out = "cert_n_different_exts.out";
    run(app(["openssl", "x509", "-noout", "-ext", $exts,
             "-in", $cert, "-out", $out]));
    is(file_n_different_lines($out), $expected, ($name ? "$name: " : "").
       "$cert '$exts' output should contain $expected different lines");
    # not unlinking $out
}

1;
