#! /usr/bin/env perl
# Copyright 2016-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

# Reads one or more template files and runs it through Text::Template
#
# It is assumed that this scripts is called with -Mconfigdata, a module
# that holds configuration data in %config

use strict;
use warnings;

use FindBin;
use Getopt::Std;

# We actually expect to get the following hash tables from configdata:
#
#    %config
#    %target
#    %withargs
#    %unified_info
#
# We just do a minimal test to see that we got what we expected.
# $config{target} must exist as an absolute minimum.
die "You must run this script with -Mconfigdata\n" if !exists($config{target});

# Make a subclass of Text::Template to override append_text_to_result,
# as recommended here:
#
# http://search.cpan.org/~mjd/Text-Template-1.46/lib/Text/Template.pm#Automatic_postprocessing_of_template_hunks

package OpenSSL::Template;

# Because we know that Text::Template isn't a core Perl module, we use
# a fallback in case it's not installed on the system
use File::Basename;
use File::Spec::Functions;
use lib "$FindBin::Bin/perl";
use with_fallback qw(Text::Template);

#use parent qw/Text::Template/;
use vars qw/@ISA/;
push @ISA, qw/Text::Template/;

# Override constructor
sub new {
    my ($class) = shift;

    # Call the constructor of the parent class, Person.
    my $self = $class->SUPER::new( @_ );
    # Add few more attributes
    $self->{_output_off}   = 0;	# Default to output hunks
    bless $self, $class;
    return $self;
}

sub append_text_to_output {
    my $self = shift;

    if ($self->{_output_off} == 0) {
	$self->SUPER::append_text_to_output(@_);
    }

    return;
}

sub output_reset_on {
    my $self = shift;
    $self->{_output_off} = 0;
}

sub output_on {
    my $self = shift;
    if (--$self->{_output_off} < 0) {
	$self->{_output_off} = 0;
    }
}

sub output_off {
    my $self = shift;
    $self->{_output_off}++;
}

# Come back to main

package main;

# Helper functions for the templates #################################

# It might be practical to quotify some strings and have them protected
# from possible harm.  These functions primarly quote things that might
# be interpreted wrongly by a perl eval.

# quotify1 STRING
# This adds quotes (") around the given string, and escapes any $, @, \,
# " and ' by prepending a \ to them.
sub quotify1 {
    my $s = my $orig = shift @_;
    $s =~ s/([\$\@\\"'])/\\$1/g;
    $s ne $orig || $s =~ /\s/ ? '"'.$s.'"' : $s;
}

# quotify_l LIST
# For each defined element in LIST (i.e. elements that aren't undef), have
# it quotified with 'quotofy1'
sub quotify_l {
    map {
        if (!defined($_)) {
            ();
        } else {
            quotify1($_);
        }
    } @_;
}

# Error reporter #####################################################

# The error reporter uses %lines to figure out exactly which file the
# error happened and at what line.  Not that the line number may be
# the start of a perl snippet rather than the exact line where it
# happened.  Nothing we can do about that here.

my %lines = ();
sub broken {
    my %args = @_;
    my $filename = "<STDIN>";
    my $deducelines = 0;
    foreach (sort keys %lines) {
        $filename = $lines{$_};
        last if ($_ > $args{lineno});
        $deducelines += $_;
    }
    print STDERR $args{error}," in $filename, fragment starting at line ",$args{lineno}-$deducelines;
    undef;
}

# Check options ######################################################

my %opts = ();

# -o ORIGINATOR
#		declares ORIGINATOR as the originating script.
getopt('o', \%opts);

my @autowarntext = ("WARNING: do not edit!",
		    "Generated"
		    . (defined($opts{o}) ? " by ".$opts{o} : "")
		    . (scalar(@ARGV) > 0 ? " from ".join(", ",@ARGV) : ""));

# Template reading ###################################################

# Read in all the templates into $text, while keeping track of each
# file and its size in lines, to try to help report errors with the
# correct file name and line number.

my $prev_linecount = 0;
my $text =
    @ARGV
    ? join("", map { my $x = Text::Template::_load_text($_);
                     if (!defined($x)) {
                         die $Text::Template::ERROR, "\n";
                     }
                     $x = "{- output_reset_on() -}" . $x;
                     my $linecount = $x =~ tr/\n//;
                     $prev_linecount = ($linecount += $prev_linecount);
                     $lines{$linecount} = $_;
                     $x } @ARGV)
    : join("", <STDIN>);

# Engage! ############################################################

# Load the full template (combination of files) into Text::Template
# and fill it up with our data.  Output goes directly to STDOUT

my $template =
    OpenSSL::Template->new(TYPE => 'STRING',
                           SOURCE => $text,
                           PREPEND => qq{use lib "$FindBin::Bin/perl";});

sub output_reset_on {
    $template->output_reset_on();
    "";
}
sub output_on {
    $template->output_on();
    "";
}
sub output_off {
    $template->output_off();
    "";
}

$template->fill_in(OUTPUT => \*STDOUT,
                   HASH => { config => \%config,
                             target => \%target,
                             disabled => \%disabled,
                             withargs => \%withargs,
                             unified_info => \%unified_info,
                             autowarntext => \@autowarntext,
                             quotify1 => \&quotify1,
                             quotify_l => \&quotify_l,
                             output_reset_on => \&output_reset_on,
                             output_on => \&output_on,
                             output_off => \&output_off },
                   DELIMITERS => [ "{-", "-}" ],
                   BROKEN => \&broken);
