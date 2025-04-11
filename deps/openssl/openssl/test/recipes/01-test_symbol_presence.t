#! /usr/bin/env perl
# -*- mode: Perl -*-
# Copyright 2016-2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use File::Spec::Functions qw(devnull);
use IPC::Cmd;
use OpenSSL::Test qw(:DEFAULT srctop_file srctop_dir bldtop_dir bldtop_file);
use OpenSSL::Test::Utils;

BEGIN {
    setup("test_symbol_presence");
}

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');
use platform;

plan skip_all => "Test is disabled on NonStop" if config('target') =~ m|^nonstop|;
# MacOS arranges symbol names differently
plan skip_all => "Test is disabled on MacOS" if config('target') =~ m|^darwin|;
plan skip_all => "This is unsupported on platforms that don't have 'nm'"
    unless IPC::Cmd::can_run('nm');

note
    "NOTE: developer test!  It's possible that it won't run on your\n",
    "platform, and that's perfectly fine.  This is mainly for developers\n",
    "on Unix to check that our shared libraries are consistent with the\n",
    "ordinals (util/*.num in the source tree), and that our static libraries\n",
    "don't share symbols, something that should be a good enough check for\n",
    "the other platforms as well.\n";

my %stlibname;
my %shlibname;
my %stlibpath;
my %shlibpath;
my %defpath;
foreach (qw(crypto ssl)) {
    $stlibname{$_} = platform->staticlib("lib$_");
    $stlibpath{$_} = bldtop_file($stlibname{$_});
    $shlibname{$_} = platform->sharedlib("lib$_") unless disabled('shared');
    $shlibpath{$_} = bldtop_file($shlibname{$_})  unless disabled('shared');
}

my $testcount
    =  1                        # Check for static library symbols duplicates
    ;
$testcount
    += (scalar keys %shlibpath) # Check for missing symbols in shared lib
    unless disabled('shared');

plan tests => $testcount;

######################################################################
# Collect symbols
# [3 tests per library]

my %stsymbols;                  # Static library symbols
my %shsymbols;                  # Shared library symbols
my %defsymbols;                 # Symbols taken from ordinals
foreach (sort keys %stlibname) {
    my $stlib_cmd = "nm -Pg $stlibpath{$_} 2> /dev/null";
    my $shlib_cmd = "nm -DPg $shlibpath{$_} 2> /dev/null";
    my @stlib_lines;
    my @shlib_lines;
    *OSTDERR = *STDERR;
    *OSTDOUT = *STDOUT;
    open STDERR, ">", devnull();
    open STDOUT, ">", devnull();
    @stlib_lines = map { s|\R$||; $_ } `$stlib_cmd`;
    if ($? != 0) {
        note "running '$stlib_cmd' => $?";
        @stlib_lines = ();
    }
    unless (disabled('shared')) {
        @shlib_lines = map { s|\R$||; $_ } `$shlib_cmd`;
        if ($? != 0) {
            note "running '$shlib_cmd' => $?";
            @shlib_lines = ();
        }
    }
    close STDERR;
    close STDOUT;
    *STDERR = *OSTDERR;
    *STDOUT = *OSTDOUT;

    my $bldtop = bldtop_dir();
    my @def_lines;
    unless (disabled('shared')) {
        indir $bldtop => sub {
            my $mkdefpath = srctop_file("util", "mkdef.pl");
            my $def_path = srctop_file("util", "lib$_.num");
            my $def_cmd = "$^X $mkdefpath --ordinals $def_path --name $_ --OS linux 2> /dev/null";
            @def_lines = map { s|\R$||; $_ } `$def_cmd`;
            if ($? != 0) {
                note "running 'cd $bldtop; $def_cmd' => $?";
                @def_lines = ();
            }
        }, create => 0, cleanup => 0;
    }

    note "Number of lines in \@stlib_lines before massaging: ", scalar @stlib_lines;
    unless (disabled('shared')) {
        note "Number of lines in \@shlib_lines before massaging: ", scalar @shlib_lines;
        note "Number of lines in \@def_lines before massaging: ", scalar @def_lines;
    }

    # Massage the nm output to only contain defined symbols
    my @arrays = ( \@stlib_lines );
    push @arrays, \@shlib_lines unless disabled('shared');
    foreach (@arrays) {
        my %commons;
        foreach (@$_) {
            if (m|^(.*) C .*|) {
                $commons{$1}++;
            }
        }
        foreach (sort keys %commons) {
            note "Common symbol: $_";
        }

        @$_ =
            sort
            ( map {
                  # Drop the first space and everything following it
                  s| .*||;
                  # Drop OpenSSL dynamic version information if there is any
                  s|\@\@.+$||;
                  # Return the result
                  $_
              }
              # Drop any symbol starting with a double underscore, they
              # are reserved for the compiler / system ABI and are none
              # of our business
              grep !m|^__|,
              # Only look at external definitions
              grep m|.* [BDST] .*|,
              @$_ ),
            keys %commons;
    }

    # Massage the mkdef.pl output to only contain global symbols
    # The output we got is in Unix .map format, which has a global
    # and a local section.  We're only interested in the global
    # section.
    my $in_global = 0;
    unless (disabled('shared')) {
        @def_lines =
            sort
            map { s|;||; s|\s+||g; $_ }
            grep { $in_global = 1 if m|global:|;
                   $in_global = 0 if m|local:|;
                   $in_global = 0 if m|\}|;
                   $in_global && m|;|; } @def_lines;
    }

    note "Number of lines in \@stlib_lines after massaging: ", scalar @stlib_lines;
    unless (disabled('shared')) {

        note "Number of lines in \@shlib_lines after massaging: ", scalar @shlib_lines;
        note "Number of lines in \@def_lines after massaging: ", scalar @def_lines;
    }

    $stsymbols{$_} = [ @stlib_lines ];
    unless (disabled('shared')) {
        $shsymbols{$_} = [ @shlib_lines ];
        $defsymbols{$_} = [ @def_lines ];
    }
}

######################################################################
# Check that there are no duplicate symbols in all our static libraries
# combined
# [1 test]

my %symbols;
foreach (sort keys %stlibname) {
    foreach (@{$stsymbols{$_}}) {
        $symbols{$_}++;
    }
}
my @duplicates = sort grep { $symbols{$_} > 1 } keys %symbols;
if (@duplicates) {
    note "Duplicates:";
    note join('\n', @duplicates);
}
ok(scalar @duplicates == 0, "checking no duplicate symbols in static libraries");

######################################################################
# Check that the exported symbols in our shared libraries are consistent
# with our ordinals files.
# [1 test per library]

unless (disabled('shared')) {
    foreach (sort keys %stlibname) {
        # Maintain lists of symbols that are missing in the shared library,
        # or that are extra.
        my @missing = ();
        my @extra = ();

        my @sh_symbols = ( @{$shsymbols{$_}} );
        my @def_symbols = ( @{$defsymbols{$_}} );

        while (scalar @sh_symbols || scalar @def_symbols) {
            my $sh_first = $sh_symbols[0];
            my $def_first = $def_symbols[0];

            if (!defined($sh_first)) {
                push @missing, shift @def_symbols;
            } elsif (!defined($def_first)) {
                push @extra, shift @sh_symbols;
            } elsif ($sh_first gt $def_first) {
                push @missing, shift @def_symbols;
            } elsif ($sh_first lt $def_first) {
                push @extra, shift @sh_symbols;
            } else {
                shift @def_symbols;
                shift @sh_symbols;
            }
        }

        if (scalar @missing) {
            note "The following symbols are missing in $_:";
            foreach (@missing) {
                note "  $_";
            }
        }
        if (scalar @extra) {
            note "The following symbols are extra in $_:";
            foreach (@extra) {
                note "  $_";
            }
        }
        ok(scalar @missing == 0,
           "check that there are no missing symbols in $_");
    }
}
