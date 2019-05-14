#! /usr/bin/env perl
# Copyright 2015-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

# Recognise VERBOSE and V which is common on other projects.
BEGIN {
    $ENV{HARNESS_VERBOSE} = "yes" if $ENV{VERBOSE} || $ENV{V};
}

use File::Spec::Functions qw/catdir catfile curdir abs2rel rel2abs/;
use File::Basename;
use FindBin;
use lib "$FindBin::Bin/../util/perl";
use OpenSSL::Glob;

my $TAP_Harness = eval { require TAP::Harness } ? "TAP::Harness"
                                                : "OpenSSL::TAP::Harness";

my $srctop = $ENV{SRCTOP} || $ENV{TOP};
my $bldtop = $ENV{BLDTOP} || $ENV{TOP};
my $recipesdir = catdir($srctop, "test", "recipes");
my $libdir = rel2abs(catdir($srctop, "util", "perl"));

$ENV{OPENSSL_CONF} = catdir($srctop, "apps", "openssl.cnf");

my %tapargs =
    ( verbosity => $ENV{VERBOSE} || $ENV{V} || $ENV{HARNESS_VERBOSE} ? 1 : 0,
      lib       => [ $libdir ],
      switches  => '-w',
      merge     => 1
    );

my @alltests = find_matching_tests("*");
my %tests = ();

my $initial_arg = 1;
foreach my $arg (@ARGV ? @ARGV : ('alltests')) {
    if ($arg eq 'list') {
	foreach (@alltests) {
	    (my $x = basename($_)) =~ s|^[0-9][0-9]-(.*)\.t$|$1|;
	    print $x,"\n";
	}
	exit 0;
    }
    if ($arg eq 'alltests') {
	warn "'alltests' encountered, ignoring everything before that...\n"
	    unless $initial_arg;
	%tests = map { $_ => 1 } @alltests;
    } elsif ($arg =~ m/^(-?)(.*)/) {
	my $sign = $1;
	my $test = $2;
	my @matches = find_matching_tests($test);

	# If '-foo' is the first arg, it's short for 'alltests -foo'
	if ($sign eq '-' && $initial_arg) {
	    %tests = map { $_ => 1 } @alltests;
	}

	if (scalar @matches == 0) {
	    warn "Test $test found no match, skipping ",
		($sign eq '-' ? "removal" : "addition"),
		"...\n";
	} else {
	    foreach $test (@matches) {
		if ($sign eq '-') {
		    delete $tests{$test};
		} else {
		    $tests{$test} = 1;
		}
	    }
	}
    } else {
	warn "I don't know what '$arg' is about, ignoring...\n";
    }

    $initial_arg = 0;
}

my $harness = $TAP_Harness->new(\%tapargs);
my $ret = $harness->runtests(map { abs2rel($_, rel2abs(curdir())); }
                                 sort keys %tests);

# $ret->has_errors may be any number, not just 0 or 1.  On VMS, numbers
# from 2 and on are used as is as VMS statuses, which has severity encoded
# in the lower 3 bits.  0 and 1, on the other hand, generate SUCCESS and
# FAILURE, so for currect reporting on all platforms, we make sure the only
# exit codes are 0 and 1.  Double-bang is the trick to do so.
exit !!$ret->has_errors if (ref($ret) eq "TAP::Parser::Aggregator");

# If this isn't a TAP::Parser::Aggregator, it's the pre-TAP test harness,
# which simply dies at the end if any test failed, so we don't need to bother
# with any exit code in that case.

sub find_matching_tests {
    my ($glob) = @_;

    if ($glob =~ m|^[\d\[\]\?\-]+$|) {
        return glob(catfile($recipesdir,"$glob-*.t"));
    }
    return glob(catfile($recipesdir,"*-$glob.t"));
}


# Fake TAP::Harness in case it's not loaded
use Test::Harness;
package OpenSSL::TAP::Harness;

sub new {
    my $class = shift;
    my %args = %{ shift() };

    return bless { %args }, $class;
}

sub runtests {
    my $self = shift;

    my @switches = ();
    if ($self->{switches}) {
	push @switches, $self->{switches};
    }
    if ($self->{lib}) {
	foreach (@{$self->{lib}}) {
	    my $l = $_;

	    # It seems that $switches is getting interpreted with 'eval' or
	    # something like that, and that we need to take care of backslashes
	    # or they will disappear along the way.
	    $l =~ s|\\|\\\\|g if $^O eq "MSWin32";
	    push @switches, "-I$l";
	}
    }

    $Test::Harness::switches = join(' ', @switches);
    Test::Harness::runtests(@_);
}
