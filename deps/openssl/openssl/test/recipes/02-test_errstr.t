#! /usr/bin/env perl
# Copyright 2018-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
no strict 'refs';               # To be able to use strings as function refs
use OpenSSL::Test;
use OpenSSL::Test::Utils;
use Errno qw(:POSIX);
use POSIX qw(:limits_h strerror);

use Data::Dumper;

setup('test_errstr');

# In a cross compiled situation, there are chances that our
# application is linked against different C libraries than
# perl, and may thereby get different error messages for the
# same error.
# The safest is not to test under such circumstances.
plan skip_all => 'This is unsupported for cross compiled configurations'
    if config('CROSS_COMPILE');

# The same can be said when compiling OpenSSL with mingw configuration
# on Windows when built with msys perl.  Similar problems are also observed
# in MSVC builds, depending on the perl implementation used.
plan skip_all => 'This is unsupported on MSYS/MinGW or MSWin32'
    if $^O eq 'msys' or $^O eq 'MSWin32';

plan skip_all => 'OpenSSL is configured "no-autoerrinit" or "no-err"'
    if disabled('autoerrinit') || disabled('err');

# OpenSSL constants found in <openssl/err.h>
use constant ERR_SYSTEM_FLAG => INT_MAX + 1;
use constant ERR_LIB_OFFSET => 23; # Offset of the "library" errcode section

# OpenSSL "library" numbers
use constant ERR_LIB_NONE => 1;

# We use Errno::EXPORT_OK as a list of known errno values on the current
# system.  libcrypto's ERR should either use the same string as perl, or if
# it was outside the range that ERR looks at, ERR gives the reason string
# "reason(nnn)", where nnn is the errno number.

plan tests => scalar @Errno::EXPORT_OK
    +1                          # Checking that error 128 gives 'reason(128)'
    +1                          # Checking that error 0 gives the library name
    +1;                         # Check trailing whitespace is removed.

# Test::More:ok() has a sub prototype, which means we need to use the '&ok'
# syntax to force it to accept a list as a series of arguments.

foreach my $errname (@Errno::EXPORT_OK) {
    # The error names are perl constants, which are implemented as functions
    # returning the numeric value of that name.
    my $errcode = "Errno::$errname"->();

  SKIP: {
      # On most systems, there is no E macro for errcode zero in <errno.h>,
      # which means that it seldom comes up here.  However, reports indicate
      # that some platforms do have an E macro for errcode zero.
      # With perl, errcode zero is a bit special.  Perl consistently gives
      # the empty string for that one, while the C strerror() may give back
      # something else.  The easiest way to deal with that possible mismatch
      # is to skip this errcode.
      skip "perl error strings and ssystem error strings for errcode 0 differ", 1
          if $errcode == 0;
      # On some systems (for example Hurd), there are negative error codes.
      # These are currently unsupported in OpenSSL error reports.
      skip "negative error codes are not supported in OpenSSL", 1
          if $errcode < 0;

      &ok(match_syserr_reason($errcode));
    }
}

# OpenSSL library 1 is the "unknown" library
&ok(match_opensslerr_reason(ERR_LIB_NONE << ERR_LIB_OFFSET | 256,
                            "reason(256)"));
# Reason code 0 of any library gives the library name as reason
&ok(match_opensslerr_reason(ERR_LIB_NONE << ERR_LIB_OFFSET |   0,
                            "unknown library"));
&ok(match_any("Trailing whitespace  \n\t", "?", ( "Trailing whitespace" )));

exit 0;

# For an error string "error:xxxxxxxx:lib:func:reason", this returns
# the following array:
#
# ( "xxxxxxxx", "lib", "func", "reason" )
sub split_error {
    # Limit to 5 items, in case the reason contains a colon
    my @erritems = split /:/, $_[0], 5;

    # Remove the first item, which is always "error"
    shift @erritems;

    return @erritems;
}

# Compares the first argument as string to each of the arguments 3 and on,
# and returns an array of two elements:
# 0:  True if the first argument matched any of the others, otherwise false
# 1:  A string describing the test
# The returned array can be used as the arguments to Test::More::ok()
sub match_any {
    my $first = shift;
    my $desc = shift;
    my @strings = @_;

    # ignore trailing whitespace
    $first =~ s/\s+$//;

    if (scalar @strings > 1) {
        $desc = "match '$first' ($desc) with one of ( '"
            . join("', '", @strings) . "' )";
    } else {
        $desc = "match '$first' ($desc) with '$strings[0]'";
    }

    return ( scalar(
                 grep { ref $_ eq 'Regexp' ? $first =~ $_ : $first eq $_ }
                 @strings
             ) > 0,
             $desc );
}

sub match_opensslerr_reason {
    my $errcode = shift;
    my @strings = @_;

    my $errcode_hex = sprintf "%x", $errcode;
    my $reason =
        ( run(app([ qw(openssl errstr), $errcode_hex ]), capture => 1) )[0];
    $reason =~ s|\R$||;
    $reason = ( split_error($reason) )[3];

    return match_any($reason, $errcode_hex, @strings);
}

sub match_syserr_reason {
    my $errcode = shift;

    my @strings = ();
    # The POSIX reason string
    push @strings, eval {
          # Set $! to the error number...
          local $! = $errcode;
          # ... and $! will give you the error string back
          $!
    };
    # Occasionally, we get an error code that is simply not translatable
    # to POSIX semantics on VMS, and we get an error string saying so.
    push @strings, qr/^non-translatable vms error code:/ if $^O eq 'VMS';
    # The OpenSSL fallback string
    push @strings, "reason($errcode)";

    return match_opensslerr_reason(ERR_SYSTEM_FLAG | $errcode, @strings);
}
