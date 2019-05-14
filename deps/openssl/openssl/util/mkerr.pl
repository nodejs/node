#! /usr/bin/env perl
# Copyright 1999-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use lib ".";
use configdata;

my $config       = "crypto/err/openssl.ec";
my $debug        = 0;
my $internal     = 0;
my $nowrite      = 0;
my $rebuild      = 0;
my $reindex      = 0;
my $static       = 0;
my $unref        = 0;
my %modules         = ();

my $errors       = 0;
my @t            = localtime();
my $YEAR         = $t[5] + 1900;

sub phase
{
    my $text = uc(shift);
    print STDERR "\n---\n$text\n" if $debug;
}

sub help
{
    print STDERR <<"EOF";
mkerr.pl [options] [files...]

Options:

    -conf FILE  Use the named config file FILE instead of the default.

    -debug      Verbose output debugging on stderr.

    -internal   Generate code that is to be built as part of OpenSSL itself.
                Also scans internal list of files.

    -module M   Only useful with -internal!
                Only write files for library module M.  Whether files are
                actually written or not depends on other options, such as
                -rebuild.
                Note: this option is cumulative.  If not given at all, all
                internal modules will be considered.

    -nowrite    Do not write the header/source files, even if changed.

    -rebuild    Rebuild all header and C source files, even if there
                were no changes.

    -reindex    Ignore previously assigned values (except for R records in
                the config file) and renumber everything starting at 100.

    -static     Make the load/unload functions static.

    -unref      List all unreferenced function and reason codes on stderr;
                implies -nowrite.

    -help       Show this help text.

    ...         Additional arguments are added to the file list to scan,
                if '-internal' was NOT specified on the command line.

EOF
}

while ( @ARGV ) {
    my $arg = $ARGV[0];
    last unless $arg =~ /-.*/;
    $arg = $1 if $arg =~ /-(-.*)/;
    if ( $arg eq "-conf" ) {
        $config = $ARGV[1];
        shift @ARGV;
    } elsif ( $arg eq "-debug" ) {
        $debug = 1;
        $unref = 1;
    } elsif ( $arg eq "-internal" ) {
        $internal = 1;
    } elsif ( $arg eq "-nowrite" ) {
        $nowrite = 1;
    } elsif ( $arg eq "-rebuild" ) {
        $rebuild = 1;
    } elsif ( $arg eq "-reindex" ) {
        $reindex = 1;
    } elsif ( $arg eq "-static" ) {
        $static = 1;
    } elsif ( $arg eq "-unref" ) {
        $unref = 1;
        $nowrite = 1;
    } elsif ( $arg eq "-module" ) {
        shift @ARGV;
        $modules{uc $ARGV[0]} = 1;
    } elsif ( $arg =~ /-*h(elp)?/ ) {
        &help();
        exit;
    } elsif ( $arg =~ /-.*/ ) {
        die "Unknown option $arg; use -h for help.\n";
    }
    shift @ARGV;
}

my @source;
if ( $internal ) {
    die "Cannot mix -internal and -static\n" if $static;
    die "Extra parameters given.\n" if @ARGV;
    @source = ( glob('crypto/*.c'), glob('crypto/*/*.c'),
                glob('ssl/*.c'), glob('ssl/*/*.c') );
} else {
    die "-module isn't useful without -internal\n" if scalar keys %modules > 0;
    @source = @ARGV;
}

# Data parsed out of the config and state files.
my %hinc;       # lib -> header
my %libinc;     # header -> lib
my %cskip;      # error_file -> lib
my %errorfile;  # lib -> error file name
my %fmax;       # lib -> max assigned function code
my %rmax;       # lib -> max assigned reason code
my %fassigned;  # lib -> colon-separated list of assigned function codes
my %rassigned;  # lib -> colon-separated list of assigned reason codes
my %fnew;       # lib -> count of new function codes
my %rnew;       # lib -> count of new reason codes
my %rextra;     # "extra" reason code -> lib
my %rcodes;     # reason-name -> value
my %ftrans;     # old name -> #define-friendly name (all caps)
my %fcodes;     # function-name -> value
my $statefile;  # state file with assigned reason and function codes
my %strings;    # define -> text

# Read and parse the config file
open(IN, "$config") || die "Can't open config file $config, $!,";
while ( <IN> ) {
    next if /^#/ || /^$/;
    if ( /^L\s+(\S+)\s+(\S+)\s+(\S+)/ ) {
        my $lib = $1;
        my $hdr = $2;
        my $err = $3;
        $hinc{$lib}   = $hdr;
        $libinc{$hdr} = $lib;
        $cskip{$err}  = $lib;
        next if $err eq 'NONE';
        $errorfile{$lib} = $err;
        $fmax{$lib}      = 100;
        $rmax{$lib}      = 100;
        $fassigned{$lib} = ":";
        $rassigned{$lib} = ":";
        $fnew{$lib}      = 0;
        $rnew{$lib}      = 0;
    } elsif ( /^R\s+(\S+)\s+(\S+)/ ) {
        $rextra{$1} = $2;
        $rcodes{$1} = $2;
    } elsif ( /^S\s+(\S+)/ ) {
        $statefile = $1;
    } else {
        die "Illegal config line $_\n";
    }
}
close IN;

if ( ! $statefile ) {
    $statefile = $config;
    $statefile =~ s/.ec/.txt/;
}

# The statefile has all the previous assignments.
&phase("Reading state");
my $skippedstate = 0;
if ( ! $reindex && $statefile ) {
    open(STATE, "<$statefile") || die "Can't open $statefile, $!";

    # Scan function and reason codes and store them: keep a note of the
    # maximum code used.
    while ( <STATE> ) {
        next if /^#/ || /^$/;
        my $name;
        my $code;
        if ( /^(.+):(\d+):\\$/ ) {
            $name = $1;
            $code = $2;
            my $next = <STATE>;
            $next =~ s/^\s*(.*)\s*$/$1/;
            die "Duplicate define $name" if exists $strings{$name};
            $strings{$name} = $next;
        } elsif ( /^(\S+):(\d+):(.*)$/ ) {
            $name = $1;
            $code = $2;
            die "Duplicate define $name" if exists $strings{$name};
            $strings{$name} = $3;
        } else {
            die "Bad line in $statefile:\n$_\n";
        }
        my $lib = $name;
        $lib =~ s/^((?:OSSL_|OPENSSL_)?[^_]{2,}).*$/$1/;
        $lib = "SSL" if $lib =~ /TLS/;
        if ( !defined $errorfile{$lib} ) {
            print "Skipping $_";
            $skippedstate++;
            next;
        }
        if ( $name =~ /^(?:OSSL_|OPENSSL_)?[A-Z0-9]{2,}_R_/ ) {
            die "$lib reason code $code collision at $name\n"
                if $rassigned{$lib} =~ /:$code:/;
            $rassigned{$lib} .= "$code:";
            if ( !exists $rextra{$name} ) {
                $rmax{$lib} = $code if $code > $rmax{$lib};
            }
            $rcodes{$name} = $code;
        } elsif ( $name =~ /^(?:OSSL_|OPENSSL_)?[A-Z0-9]{2,}_F_/ ) {
            die "$lib function code $code collision at $name\n"
                if $fassigned{$lib} =~ /:$code:/;
            $fassigned{$lib} .= "$code:";
            $fmax{$lib} = $code if $code > $fmax{$lib};
            $fcodes{$name} = $code;
        } else {
            die "Bad line in $statefile:\n$_\n";
        }
    }
    close(STATE);

    if ( $debug ) {
        foreach my $lib ( sort keys %rmax ) {
            print STDERR "Reason codes for ${lib}:\n";
            if ( $rassigned{$lib} =~ m/^:(.*):$/ ) {
                my @rassigned = sort { $a <=> $b } split( ":", $1 );
                print STDERR "  ", join(' ', @rassigned), "\n";
            } else {
                print STDERR "  --none--\n";
            }
        }
        print STDERR "\n";
        foreach my $lib ( sort keys %fmax ) {
            print STDERR "Function codes for ${lib}:\n";
            if ( $fassigned{$lib} =~ m/^:(.*):$/ ) {
                my @fassigned = sort { $a <=> $b } split( ":", $1 );
                print STDERR "  ", join(' ', @fassigned), "\n";
            } else {
                print STDERR "  --none--\n";
            }
        }
    }
}

# Scan each header file and make a list of error codes
# and function names
&phase("Scanning headers");
while ( ( my $hdr, my $lib ) = each %libinc ) {
    next if $hdr eq "NONE";
    print STDERR " ." if $debug;
    my $line = "";
    my $def = "";
    my $linenr = 0;
    my $cpp = 0;

    open(IN, "<$hdr") || die "Can't open $hdr, $!,";
    while ( <IN> ) {
        $linenr++;

        if ( $line ne '' ) {
            $_    = $line . $_;
            $line = '';
        }

        if ( /\\$/ ) {
            $line = $_;
            next;
        }

        if ( /\/\*/ ) {
            if ( not /\*\// ) {    # multiline comment...
                $line = $_;        # ... just accumulate
                next;
            } else {
                s/\/\*.*?\*\///gs;    # wipe it
            }
        }

        if ( $cpp ) {
            $cpp++ if /^#\s*if/;
            $cpp-- if /^#\s*endif/;
            next;
        }
        $cpp = 1 if /^#.*ifdef.*cplusplus/;    # skip "C" declaration

        next if /^\#/;    # skip preprocessor directives

        s/{[^{}]*}//gs;     # ignore {} blocks

        if ( /\{|\/\*/ ) {    # Add a so editor works...
            $line = $_;
        } else {
            $def .= $_;
        }
    }

    # Delete any DECLARE_ macros
    my $defnr = 0;
    $def =~ s/DECLARE_\w+\([\w,\s]+\)//gs;
    foreach ( split /;/, $def ) {
        $defnr++;
        # The goal is to collect function names from function declarations.

        s/^[\n\s]*//g;
        s/[\n\s]*$//g;

        # Skip over recognized non-function declarations
        next if /typedef\W/ or /DECLARE_STACK_OF/ or /TYPEDEF_.*_OF/;

        # Remove STACK_OF(foo)
        s/STACK_OF\(\w+\)/void/;

        # Reduce argument lists to empty ()
        # fold round brackets recursively: (t(*v)(t),t) -> (t{}{},t) -> {}
        while ( /\(.*\)/s ) {
            s/\([^\(\)]+\)/\{\}/gs;
            s/\(\s*\*\s*(\w+)\s*\{\}\s*\)/$1/gs;    #(*f{}) -> f
        }

        # pretend as we didn't use curly braces: {} -> ()
        s/\{\}/\(\)/gs;

        # Last token just before the first () is a function name.
        if ( /(\w+)\s*\(\).*/s ) {
            my $name = $1;
            $name =~ tr/[a-z]/[A-Z]/;
            $ftrans{$name} = $1;
        } elsif ( /[\(\)]/ and not(/=/) ) {
            print STDERR "Header $hdr: cannot parse: $_;\n";
        }
    }

    next if $reindex;

    if ( $lib eq "SSL" && $rmax{$lib} >= 1000 ) {
        print STDERR "SSL error codes 1000+ are reserved for alerts.\n";
        print STDERR "Any new alerts must be added to $config.\n";
        $errors++;
    }
    close IN;
}
print STDERR "\n" if $debug;

# Scan each C source file and look for function and reason codes
# This is done by looking for strings that "look like" function or
# reason codes: basically anything consisting of all upper case and
# numerics which has _F_ or _R_ in it and which has the name of an
# error library at the start.  This seems to work fine except for the
# oddly named structure BIO_F_CTX which needs to be ignored.
# If a code doesn't exist in list compiled from headers then mark it
# with the value "X" as a place holder to give it a value later.
# Store all function and reason codes found in %usedfuncs and %usedreasons
# so all those unreferenced can be printed out.
&phase("Scanning source");
my %usedfuncs;
my %usedreasons;
foreach my $file ( @source ) {
    # Don't parse the error source file.
    next if exists $cskip{$file};
    open( IN, "<$file" ) || die "Can't open $file, $!,";
    my $func;
    my $linenr = 0;
    print STDERR "$file:\n" if $debug;
    while ( <IN> ) {

        # skip obsoleted source files entirely!
        last if /^#error\s+obsolete/;
        $linenr++;
        if ( !/;$/ && /^\**([a-zA-Z_].*[\s*])?([A-Za-z_0-9]+)\(.*([),]|$)/ ) {
            /^([^()]*(\([^()]*\)[^()]*)*)\(/;
            $1 =~ /([A-Za-z_0-9]*)$/;
            $func = $1;
        }

        if ( /(((?:OSSL_|OPENSSL_)?[A-Z0-9]{2,})_F_([A-Z0-9_]+))/ ) {
            next unless exists $errorfile{$2};
            next if $1 eq "BIO_F_BUFFER_CTX";
            $usedfuncs{$1} = 1;
            if ( !exists $fcodes{$1} ) {
                print STDERR "  New function $1\n" if $debug;
                $fcodes{$1} = "X";
                $fnew{$2}++;
            }
            $ftrans{$3} = $func unless exists $ftrans{$3};
            if ( uc($func) ne $3 ) {
                print STDERR "ERROR: mismatch $file:$linenr $func:$3\n";
                $errors++;
            }
            print STDERR "  Function $1 = $fcodes{$1}\n"
              if $debug;
        }
        if ( /(((?:OSSL_|OPENSSL_)?[A-Z0-9]{2,})_R_[A-Z0-9_]+)/ ) {
            next unless exists $errorfile{$2};
            $usedreasons{$1} = 1;
            if ( !exists $rcodes{$1} ) {
                print STDERR "  New reason $1\n" if $debug;
                $rcodes{$1} = "X";
                $rnew{$2}++;
            }
            print STDERR "  Reason $1 = $rcodes{$1}\n" if $debug;
        }
    }
    close IN;
}
print STDERR "\n" if $debug;

# Now process each library in turn.
&phase("Writing files");
my $newstate = 0;
foreach my $lib ( keys %errorfile ) {
    if ( ! $fnew{$lib} && ! $rnew{$lib} ) {
        next unless $rebuild;
    }
    next if scalar keys %modules > 0 && !$modules{$lib};
    next if $nowrite;
    print STDERR "$lib: $fnew{$lib} new functions\n" if $fnew{$lib};
    print STDERR "$lib: $rnew{$lib} new reasons\n" if $rnew{$lib};
    $newstate = 1;

    # If we get here then we have some new error codes so we
    # need to rebuild the header file and C file.

    # Make a sorted list of error and reason codes for later use.
    my @function = sort grep( /^${lib}_/, keys %fcodes );
    my @reasons  = sort grep( /^${lib}_/, keys %rcodes );

    # indent level for innermost preprocessor lines
    my $indent = " ";

    # Rewrite the header file

    my $hfile = $hinc{$lib};
    $hfile =~ s/.h$/err.h/ if $internal;
    open( OUT, ">$hfile" ) || die "Can't write to $hfile, $!,";
    print OUT <<"EOF";
/*
 * Generated by util/mkerr.pl DO NOT EDIT
 * Copyright 1995-$YEAR The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the \"License\").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef HEADER_${lib}ERR_H
# define HEADER_${lib}ERR_H

EOF
    if ( $internal ) {
        # Declare the load function because the generate C file
        # includes "fooerr.h" not "foo.h"
        if ($lib ne "SSL" && $lib ne "ASYNC"
                && grep { $lib eq uc $_ } @disablables) {
            print OUT <<"EOF";
# include <openssl/opensslconf.h>

# ifndef OPENSSL_NO_${lib}

EOF
            $indent = "  ";
        }
        print OUT <<"EOF";
#${indent}ifdef  __cplusplus
extern \"C\"
#${indent}endif
int ERR_load_${lib}_strings(void);
EOF
    } else {
        print OUT <<"EOF";
# define ${lib}err(f, r) ERR_${lib}_error((f), (r), OPENSSL_FILE, OPENSSL_LINE)

EOF
        if ( ! $static ) {
            print OUT <<"EOF";

# ifdef  __cplusplus
extern \"C\" {
# endif
int ERR_load_${lib}_strings(void);
void ERR_unload_${lib}_strings(void);
void ERR_${lib}_error(int function, int reason, char *file, int line);
# ifdef  __cplusplus
}
# endif
EOF
        }
    }

    print OUT "\n/*\n * $lib function codes.\n */\n";
    foreach my $i ( @function ) {
        my $z = 48 - length($i);
        $z = 0 if $z < 0;
        if ( $fcodes{$i} eq "X" ) {
            $fassigned{$lib} =~ m/^:([^:]*):/;
            my $findcode = $1;
            $findcode = $fmax{$lib} if !defined $findcode;
            while ( $fassigned{$lib} =~ m/:$findcode:/ ) {
                $findcode++;
            }
            $fcodes{$i} = $findcode;
            $fassigned{$lib} .= "$findcode:";
            print STDERR "New Function code $i\n" if $debug;
        }
        printf OUT "#${indent}define $i%s $fcodes{$i}\n", " " x $z;
    }

    print OUT "\n/*\n * $lib reason codes.\n */\n";
    foreach my $i ( @reasons ) {
        my $z = 48 - length($i);
        $z = 0 if $z < 0;
        if ( $rcodes{$i} eq "X" ) {
            $rassigned{$lib} =~ m/^:([^:]*):/;
            my $findcode = $1;
            $findcode = $rmax{$lib} if !defined $findcode;
            while ( $rassigned{$lib} =~ m/:$findcode:/ ) {
                $findcode++;
            }
            $rcodes{$i} = $findcode;
            $rassigned{$lib} .= "$findcode:";
            print STDERR "New Reason code $i\n" if $debug;
        }
        printf OUT "#${indent}define $i%s $rcodes{$i}\n", " " x $z;
    }
    print OUT "\n";

    while (length($indent) > 0) {
        $indent = substr $indent, 0, -1;
        print OUT "#${indent}endif\n";
    }

    # Rewrite the C source file containing the error details.

    # First, read any existing reason string definitions:
    my $cfile = $errorfile{$lib};
    my $pack_lib = $internal ? "ERR_LIB_${lib}" : "0";
    my $hincf = $hfile;
    $hincf =~ s|.*include/||;
    if ( $hincf =~ m|^openssl/| ) {
        $hincf = "<${hincf}>";
    } else {
        $hincf = "\"${hincf}\"";
    }

    open( OUT, ">$cfile" )
        || die "Can't open $cfile for writing, $!, stopped";

    my $const = $internal ? 'const ' : '';

    print OUT <<"EOF";
/*
 * Generated by util/mkerr.pl DO NOT EDIT
 * Copyright 1995-$YEAR The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/err.h>
#include $hincf

#ifndef OPENSSL_NO_ERR

static ${const}ERR_STRING_DATA ${lib}_str_functs[] = {
EOF

    # Add each function code: if a function name is found then use it.
    foreach my $i ( @function ) {
        my $fn;
        if ( exists $strings{$i} and $strings{$i} ne '' ) {
            $fn = $strings{$i};
            $fn = "" if $fn eq '*';
        } else {
            $i =~ /^${lib}_F_(\S+)$/;
            $fn = $1;
            $fn = $ftrans{$fn} if exists $ftrans{$fn};
            $strings{$i} = $fn;
        }
        my $short = "    {ERR_PACK($pack_lib, $i, 0), \"$fn\"},";
        if ( length($short) <= 80 ) {
            print OUT "$short\n";
        } else {
            print OUT "    {ERR_PACK($pack_lib, $i, 0),\n     \"$fn\"},\n";
        }
    }
    print OUT <<"EOF";
    {0, NULL}
};

static ${const}ERR_STRING_DATA ${lib}_str_reasons[] = {
EOF

    # Add each reason code.
    foreach my $i ( @reasons ) {
        my $rn;
        if ( exists $strings{$i} ) {
            $rn = $strings{$i};
            $rn = "" if $rn eq '*';
        } else {
            $i =~ /^${lib}_R_(\S+)$/;
            $rn = $1;
            $rn =~ tr/_[A-Z]/ [a-z]/;
            $strings{$i} = $rn;
        }
        my $short = "    {ERR_PACK($pack_lib, 0, $i), \"$rn\"},";
        if ( length($short) <= 80 ) {
            print OUT "$short\n";
        } else {
            print OUT "    {ERR_PACK($pack_lib, 0, $i),\n    \"$rn\"},\n";
        }
    }
    print OUT <<"EOF";
    {0, NULL}
};

#endif
EOF
    if ( $internal ) {
        print OUT <<"EOF";

int ERR_load_${lib}_strings(void)
{
#ifndef OPENSSL_NO_ERR
    if (ERR_func_error_string(${lib}_str_functs[0].error) == NULL) {
        ERR_load_strings_const(${lib}_str_functs);
        ERR_load_strings_const(${lib}_str_reasons);
    }
#endif
    return 1;
}
EOF
    } else {
        my $st = $static ? "static " : "";
        print OUT <<"EOF";

static int lib_code = 0;
static int error_loaded = 0;

${st}int ERR_load_${lib}_strings(void)
{
    if (lib_code == 0)
        lib_code = ERR_get_next_error_library();

    if (!error_loaded) {
#ifndef OPENSSL_NO_ERR
        ERR_load_strings(lib_code, ${lib}_str_functs);
        ERR_load_strings(lib_code, ${lib}_str_reasons);
#endif
        error_loaded = 1;
    }
    return 1;
}

${st}void ERR_unload_${lib}_strings(void)
{
    if (error_loaded) {
#ifndef OPENSSL_NO_ERR
        ERR_unload_strings(lib_code, ${lib}_str_functs);
        ERR_unload_strings(lib_code, ${lib}_str_reasons);
#endif
        error_loaded = 0;
    }
}

${st}void ERR_${lib}_error(int function, int reason, char *file, int line)
{
    if (lib_code == 0)
        lib_code = ERR_get_next_error_library();
    ERR_PUT_error(lib_code, function, reason, file, line);
}
EOF

    }

    close OUT;
}

&phase("Ending");
# Make a list of unreferenced function and reason codes
if ( $unref ) {
    my @funref;
    foreach ( keys %fcodes ) {
        push( @funref, $_ ) unless exists $usedfuncs{$_};
    }
    my @runref;
    foreach ( keys %rcodes ) {
        push( @runref, $_ ) unless exists $usedreasons{$_};
    }
    if ( @funref ) {
        print STDERR "The following function codes were not referenced:\n";
        foreach ( sort @funref ) {
            print STDERR "  $_\n";
        }
    }
    if ( @runref ) {
        print STDERR "The following reason codes were not referenced:\n";
        foreach ( sort @runref ) {
            print STDERR "  $_\n";
        }
    }
}

die "Found $errors errors, quitting" if $errors;

# Update the state file
if ( $newstate )  {
    open(OUT, ">$statefile.new")
        || die "Can't write $statefile.new, $!";
    print OUT <<"EOF";
# Copyright 1999-$YEAR The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html
EOF
    print OUT "\n# Function codes\n";
    foreach my $i ( sort keys %fcodes ) {
        my $short = "$i:$fcodes{$i}:";
        my $t = exists $strings{$i} ? $strings{$i} : "";
        $t = "\\\n\t" . $t if length($short) + length($t) > 80;
        print OUT "$short$t\n";
    }
    print OUT "\n#Reason codes\n";
    foreach my $i ( sort keys %rcodes ) {
        my $short = "$i:$rcodes{$i}:";
        my $t = exists $strings{$i} ? "$strings{$i}" : "";
        $t = "\\\n\t" . $t if length($short) + length($t) > 80;
        print OUT "$short$t\n" if !exists $rextra{$i};
    }
    close(OUT);
    if ( $skippedstate ) {
        print "Skipped state, leaving update in $statefile.new";
    } else {
        rename "$statefile", "$statefile.old"
            || die "Can't backup $statefile to $statefile.old, $!";
        rename "$statefile.new", "$statefile"
            || die "Can't rename $statefile to $statefile.new, $!";
    }
}

exit;
