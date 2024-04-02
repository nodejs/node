#! /usr/bin/env perl
# Copyright 2018-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

package OpenSSL::Util;

use strict;
use warnings;
use Carp;

use Exporter;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
$VERSION = "0.1";
@ISA = qw(Exporter);
@EXPORT = qw(cmp_versions quotify1 quotify_l fixup_cmd_elements fixup_cmd
             dump_data);
@EXPORT_OK = qw();

=head1 NAME

OpenSSL::Util - small OpenSSL utilities

=head1 SYNOPSIS

  use OpenSSL::Util;

  $versiondiff = cmp_versions('1.0.2k', '3.0.1');
  # $versiondiff should be -1

  $versiondiff = cmp_versions('1.1.0', '1.0.2a');
  # $versiondiff should be 1

  $versiondiff = cmp_versions('1.1.1', '1.1.1');
  # $versiondiff should be 0

=head1 DESCRIPTION

=over

=item B<cmp_versions "VERSION1", "VERSION2">

Compares VERSION1 with VERSION2, paying attention to OpenSSL versioning.

Returns 1 if VERSION1 is greater than VERSION2, 0 if they are equal, and
-1 if VERSION1 is less than VERSION2.

=back

=cut

# Until we're rid of everything with the old version scheme,
# we need to be able to handle older style x.y.zl versions.
# In terms of comparison, the x.y.zl and the x.y.z schemes
# are compatible...  mostly because the latter starts at a
# new major release with a new major number.
sub _ossl_versionsplit {
    my $textversion = shift;
    return $textversion if $textversion eq '*';
    my ($major,$minor,$edit,$letter) =
        $textversion =~ /^(\d+)\.(\d+)\.(\d+)([a-z]{0,2})$/;

    return ($major,$minor,$edit,$letter);
}

sub cmp_versions {
    my @a_split = _ossl_versionsplit(shift);
    my @b_split = _ossl_versionsplit(shift);
    my $verdict = 0;

    while (@a_split) {
        # The last part is a letter sequence (or a '*')
        if (scalar @a_split == 1) {
            $verdict = $a_split[0] cmp $b_split[0];
        } else {
            $verdict = $a_split[0] <=> $b_split[0];
        }
        shift @a_split;
        shift @b_split;
        last unless $verdict == 0;
    }

    return $verdict;
}

# It might be practical to quotify some strings and have them protected
# from possible harm.  These functions primarily quote things that might
# be interpreted wrongly by a perl eval.

=over 4

=item quotify1 STRING

This adds quotes (") around the given string, and escapes any $, @, \,
" and ' by prepending a \ to them.

=back

=cut

sub quotify1 {
    my $s = shift @_;
    $s =~ s/([\$\@\\"'])/\\$1/g;
    '"'.$s.'"';
}

=over 4

=item quotify_l LIST

For each defined element in LIST (i.e. elements that aren't undef), have
it quotified with 'quotify1'.
Undefined elements are ignored.

=cut

sub quotify_l {
    map {
        if (!defined($_)) {
            ();
        } else {
            quotify1($_);
        }
    } @_;
}

=over 4

=item fixup_cmd_elements LIST

Fixes up the command line elements given by LIST in a platform specific
manner.

The result of this function is a copy of LIST with strings where quotes and
escapes have been injected as necessary depending on the content of each
LIST string.

This can also be used to put quotes around the executable of a command.
I<This must never ever be done on VMS.>

=back

=cut

sub fixup_cmd_elements {
    # A formatter for the command arguments, defaulting to the Unix setup
    my $arg_formatter =
        sub { $_ = shift;
              ($_ eq '' || /\s|[\{\}\\\$\[\]\*\?\|\&:;<>]/) ? "'$_'" : $_ };

    if ( $^O eq "VMS") {        # VMS setup
        $arg_formatter = sub {
            $_ = shift;
            if ($_ eq '' || /\s|[!"[:upper:]]/) {
                s/"/""/g;
                '"'.$_.'"';
            } else {
                $_;
            }
        };
    } elsif ( $^O eq "MSWin32") { # MSWin setup
        $arg_formatter = sub {
            $_ = shift;
            if ($_ eq '' || /\s|["\|\&\*\;<>]/) {
                s/(["\\])/\\$1/g;
                '"'.$_.'"';
            } else {
                $_;
            }
        };
    }

    return ( map { $arg_formatter->($_) } @_ );
}

=over 4

=item fixup_cmd LIST

This is a sibling of fixup_cmd_elements() that expects the LIST to be a
complete command line.  It does the same thing as fixup_cmd_elements(),
expect that it treats the first LIST element specially on VMS.

=back

=cut

sub fixup_cmd {
    return fixup_cmd_elements(@_) unless $^O eq 'VMS';

    # The rest is VMS specific
    my $prog = shift;

    # On VMS, running random executables without having a command symbol
    # means running them with the MCR command.  This is an old PDP-11
    # command that stuck around.
    # This assumes that we're passed the name of an executable.  This is a
    # safe assumption for OpenSSL command lines
    my $prefix = 'MCR';

    if ($prog =~ /^MCR$/i) {
        # If the first element is "MCR" (independent of case) already, then
        # we assume that the program it runs is already written the way it
        # should, and just grab it.
        $prog = shift;
    } else {
        # If the command itself doesn't have a directory spec, make sure
        # that there is one.  Otherwise, MCR assumes that the program
        # resides in SYS$SYSTEM:
        $prog = '[]' . $prog unless $prog =~ /^(?:[\$a-z0-9_]+:)?[<\[]/i;
    }

    return ( $prefix, $prog, fixup_cmd_elements(@_) );
}

=item dump_data REF, OPTS

Dump the data from REF into a string that can be evaluated into the same
data by Perl.

OPTS is the rest of the arguments, expected to be pairs formed with C<< => >>.
The following OPTS keywords are understood:

=over 4

=item B<delimiters =E<gt> 0 | 1>

Include the outer delimiter of the REF type in the resulting string if C<1>,
otherwise not.

=item B<indent =E<gt> num>

The indentation of the caller, i.e. an initial value.  If not given, there
will be no indentation at all, and the string will only be one line.

=back

=cut

sub dump_data {
    my $ref = shift;
    # Available options:
    # indent           => callers indentation ( undef for no indentation,
    #                     an integer otherwise )
    # delimiters       => 1 if outer delimiters should be added
    my %opts = @_;

    my $indent = $opts{indent} // 1;
    # Indentation of the whole structure, where applicable
    my $nlindent1 = defined $opts{indent} ? "\n" . ' ' x $indent : ' ';
    # Indentation of individual items, where applicable
    my $nlindent2 = defined $opts{indent} ? "\n" . ' ' x ($indent + 4) : ' ';
    my %subopts = ();

    $subopts{delimiters} = 1;
    $subopts{indent} = $opts{indent} + 4 if defined $opts{indent};

    my $product;      # Finished product, or reference to a function that
                      # produces a string, given $_
    # The following are only used when $product is a function reference
    my $delim_l;      # Left delimiter of structure
    my $delim_r;      # Right delimiter of structure
    my $separator;    # Item separator
    my @items;        # Items to iterate over

     if (ref($ref) eq "ARRAY") {
         if (scalar @$ref == 0) {
             $product = $opts{delimiters} ? '[]' : '';
         } else {
             $product = sub {
                 dump_data(\$_, %subopts)
             };
             $delim_l = ($opts{delimiters} ? '[' : '').$nlindent2;
             $delim_r = $nlindent1.($opts{delimiters} ? ']' : '');
             $separator = ",$nlindent2";
             @items = @$ref;
         }
     } elsif (ref($ref) eq "HASH") {
         if (scalar keys %$ref == 0) {
             $product = $opts{delimiters} ? '{}' : '';
         } else {
             $product = sub {
                 quotify1($_) . " => " . dump_data($ref->{$_}, %subopts);
             };
             $delim_l = ($opts{delimiters} ? '{' : '').$nlindent2;
             $delim_r = $nlindent1.($opts{delimiters} ? '}' : '');
             $separator = ",$nlindent2";
             @items = sort keys %$ref;
         }
     } elsif (ref($ref) eq "SCALAR") {
         $product = defined $$ref ? quotify1 $$ref : "undef";
     } else {
         $product = defined $ref ? quotify1 $ref : "undef";
     }

     if (ref($product) eq "CODE") {
         $delim_l . join($separator, map { &$product } @items) . $delim_r;
     } else {
         $product;
     }
}

=back

=cut

1;
