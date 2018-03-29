#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
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
use Module::Load::Conditional qw(can_load);

my $TAP_Harness = can_load(modules => { 'TAP::Harness' => undef }) 
    ? 'TAP::Harness' : 'OpenSSL::TAP::Harness';

my $srctop = $ENV{SRCTOP} || $ENV{TOP};
my $bldtop = $ENV{BLDTOP} || $ENV{TOP};
my $recipesdir = catdir($srctop, "test", "recipes");
my $libdir = rel2abs(catdir($srctop, "util", "perl"));

my %tapargs =
    ( verbosity => $ENV{VERBOSE} || $ENV{V} || $ENV{HARNESS_VERBOSE} ? 1 : 0,
      lib       => [ $libdir ],
      switches  => '-w',
      merge     => 1
    );

my @tests = ( "alltests" );
if (@ARGV) {
    @tests = @ARGV;
}
my $list_mode = scalar(grep /^list$/, @tests) != 0;
if (grep /^(alltests|list)$/, @tests) {
    @tests = grep {
	basename($_) =~ /^[0-9][0-9]-[^\.]*\.t$/
    } glob(catfile($recipesdir,"*.t"));
} else {
    my @t = ();
    foreach (@tests) {
	push @t, grep {
	    basename($_) =~ /^[0-9][0-9]-[^\.]*\.t$/
	} glob(catfile($recipesdir,"*-$_.t"));
    }
    @tests = @t;
}

if ($list_mode) {
    @tests = map { $_ = basename($_); $_ =~ s/^[0-9][0-9]-//; $_ =~ s/\.t$//;
                   $_ } @tests;
    print join("\n", @tests), "\n";
} else {
    @tests = map { abs2rel($_, rel2abs(curdir())); } @tests;

    my $harness = $TAP_Harness->new(\%tapargs);
    my $ret = $harness->runtests(sort @tests);

    # $ret->has_errors may be any number, not just 0 or 1.  On VMS, numbers
    # from 2 and on are used as is as VMS statuses, which has severity encoded
    # in the lower 3 bits.  0 and 1, on the other hand, generate SUCCESS and
    # FAILURE, so for currect reporting on all platforms, we make sure the only
    # exit codes are 0 and 1.  Double-bang is the trick to do so.
    exit !!$ret->has_errors if (ref($ret) eq "TAP::Parser::Aggregator");

    # If this isn't a TAP::Parser::Aggregator, it's the pre-TAP test harness,
    # which simply dies at the end if any test failed, so we don't need to
    # bother with any exit code in that case.
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
