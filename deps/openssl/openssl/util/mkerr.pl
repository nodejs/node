#!/usr/local/bin/perl -w

my $config = "crypto/err/openssl.ec";
my $hprefix = "openssl/";
my $debug = 0;
my $rebuild = 0;
my $static = 1;
my $recurse = 0;
my $reindex = 0;
my $dowrite = 0;
my $staticloader = "";

my $pack_errcode;
my $load_errcode;

my $errcount;

while (@ARGV) {
	my $arg = $ARGV[0];
	if($arg eq "-conf") {
		shift @ARGV;
		$config = shift @ARGV;
	} elsif($arg eq "-hprefix") {
		shift @ARGV;
		$hprefix = shift @ARGV;
	} elsif($arg eq "-debug") {
		$debug = 1;
		shift @ARGV;
	} elsif($arg eq "-rebuild") {
		$rebuild = 1;
		shift @ARGV;
	} elsif($arg eq "-recurse") {
		$recurse = 1;
		shift @ARGV;
	} elsif($arg eq "-reindex") {
		$reindex = 1;
		shift @ARGV;
	} elsif($arg eq "-nostatic") {
		$static = 0;
		shift @ARGV;
	} elsif($arg eq "-staticloader") {
		$staticloader = "static ";
		shift @ARGV;
	} elsif($arg eq "-write") {
		$dowrite = 1;
		shift @ARGV;
	} elsif($arg eq "-help" || $arg eq "-h" || $arg eq "-?" || $arg eq "--help") {
		print STDERR <<"EOF";
mkerr.pl [options] ...

Options:

  -conf F       Use the config file F instead of the default one:
                  crypto/err/openssl.ec

  -hprefix P    Prepend the filenames in generated #include <header>
                statements with prefix P. Default: 'openssl/' (without
                the quotes, naturally)

  -debug        Turn on debugging verbose output on stderr.

  -rebuild      Rebuild all header and C source files, irrespective of the
                fact if any error or function codes have been added/removed.
                Default: only update files for libraries which saw change
                         (of course, this requires '-write' as well, or no
                          files will be touched!)

  -recurse      scan a preconfigured set of directories / files for error and
                function codes:
                  (<crypto/*.c>, <crypto/*/*.c>, <ssl/*.c>, <apps/*.c>)
                When this option is NOT specified, the filelist is taken from
                the commandline instead. Here, wildcards may be embedded. (Be
                sure to escape those to prevent the shell from expanding them
                for you when you wish mkerr.pl to do so instead.)
                Default: take file list to scan from the command line.

  -reindex      Discard the numeric values previously assigned to the error
                and function codes as extracted from the scanned header files;
                instead renumber all of them starting from 100. (Note that
                the numbers assigned through 'R' records in the config file
                remain intact.)
                Default: keep previously assigned numbers. (You are warned
                         when collisions are detected.)

  -nostatic     Generates a different source code, where these additional 
                functions are generated for each library specified in the
                config file:
                  void ERR_load_<LIB>_strings(void);
                  void ERR_unload_<LIB>_strings(void);
                  void ERR_<LIB>_error(int f, int r, char *fn, int ln);
                  #define <LIB>err(f,r) ERR_<LIB>_error(f,r,__FILE__,__LINE__)
                while the code facilitates the use of these in an environment
                where the error support routines are dynamically loaded at 
                runtime.
                Default: 'static' code generation.

  -staticloader Prefix generated functions with the 'static' scope modifier.
                Default: don't write any scope modifier prefix.

  -write        Actually (over)write the generated code to the header and C 
                source files as assigned to each library through the config 
                file.
                Default: don't write.

  -help / -h / -? / --help            Show this help text.

  ...           Additional arguments are added to the file list to scan,
                assuming '-recurse' was NOT specified on the command line.

EOF
		exit 1;
	} else {
		last;
	}
}

if($recurse) {
	@source = (<crypto/*.c>, <crypto/*/*.c>, <ssl/*.c>);
} else {
	@source = @ARGV;
}

# Read in the config file

open(IN, "<$config") || die "Can't open config file $config";

# Parse config file

while(<IN>)
{
	if(/^L\s+(\S+)\s+(\S+)\s+(\S+)/) {
		$hinc{$1} = $2;
		$libinc{$2} = $1;
		$cskip{$3} = $1;
		if($3 ne "NONE") {
			$csrc{$1} = $3;
			$fmax{$1} = 100;
			$rmax{$1} = 100;
			$fassigned{$1} = ":";
			$rassigned{$1} = ":";
			$fnew{$1} = 0;
			$rnew{$1} = 0;
		}
	} elsif (/^F\s+(\S+)/) {
	# Add extra function with $1
	} elsif (/^R\s+(\S+)\s+(\S+)/) {
		$rextra{$1} = $2;
		$rcodes{$1} = $2;
	}
}

close IN;

# Scan each header file in turn and make a list of error codes
# and function names

while (($hdr, $lib) = each %libinc)
{
	next if($hdr eq "NONE");
	print STDERR "Scanning header file $hdr\n" if $debug; 
	my $line = "", $def= "", $linenr = 0, $gotfile = 0;
	if (open(IN, "<$hdr")) {
	    $gotfile = 1;
	    while(<IN>) {
		$linenr++;
		print STDERR "line: $linenr\r" if $debug;

		last if(/BEGIN\s+ERROR\s+CODES/);
		if ($line ne '') {
		    $_ = $line . $_;
		    $line = '';
		}

		if (/\\$/) {
		    $line = $_;
		    next;
		}

		if(/\/\*/) {
		    if (not /\*\//) {		# multiline comment...
			$line = $_;		# ... just accumulate
			next; 
		    } else {
			s/\/\*.*?\*\///gs;	# wipe it
		    }
		}

		if ($cpp) {
		    $cpp++ if /^#\s*if/;
		    $cpp-- if /^#\s*endif/;
		    next;
		}
		$cpp = 1 if /^#.*ifdef.*cplusplus/;  # skip "C" declaration

		next if (/^\#/);                      # skip preprocessor directives

		s/{[^{}]*}//gs;                      # ignore {} blocks

		if (/\{|\/\*/) { # Add a } so editor works...
		    $line = $_;
		} else {
		    $def .= $_;
		}
	    }
	}

	print STDERR "                                  \r" if $debug;
        $defnr = 0;
	# Delete any DECLARE_ macros
	$def =~ s/DECLARE_\w+\([\w,\s]+\)//gs;
	foreach (split /;/, $def) {
	    $defnr++;
	    print STDERR "def: $defnr\r" if $debug;

	    # The goal is to collect function names from function declarations.

	    s/^[\n\s]*//g;
	    s/[\n\s]*$//g;

	    # Skip over recognized non-function declarations
	    next if(/typedef\W/ or /DECLARE_STACK_OF/ or /TYPEDEF_.*_OF/);

	    # Remove STACK_OF(foo)
	    s/STACK_OF\(\w+\)/void/;

	    # Reduce argument lists to empty ()
	    # fold round brackets recursively: (t(*v)(t),t) -> (t{}{},t) -> {}
	    while(/\(.*\)/s) {
		s/\([^\(\)]+\)/\{\}/gs;
		s/\(\s*\*\s*(\w+)\s*\{\}\s*\)/$1/gs;	#(*f{}) -> f
	    }
	    # pretend as we didn't use curly braces: {} -> ()
	    s/\{\}/\(\)/gs;

	    if (/(\w+)\s*\(\).*/s) {	# first token prior [first] () is
		my $name = $1;		# a function name!
		$name =~ tr/[a-z]/[A-Z]/;
		$ftrans{$name} = $1;
	    } elsif (/[\(\)]/ and not (/=/)) {
		print STDERR "Header $hdr: cannot parse: $_;\n";
	    }
	}

	print STDERR "                                  \r" if $debug;

	next if $reindex;

	# Scan function and reason codes and store them: keep a note of the
	# maximum code used.

	if ($gotfile) {
	  while(<IN>) {
		if(/^\#define\s+(\S+)\s+(\S+)/) {
			$name = $1;
			$code = $2;
			next if $name =~ /^${lib}err/;
			unless($name =~ /^${lib}_([RF])_(\w+)$/) {
				print STDERR "Invalid error code $name\n";
				next;
			}
			if($1 eq "R") {
				$rcodes{$name} = $code;
				if ($rassigned{$lib} =~ /:$code:/) {
					print STDERR "!! ERROR: $lib reason code $code assigned twice (collision at $name)\n";
					++$errcount;
				}
				$rassigned{$lib} .= "$code:";
				if(!(exists $rextra{$name}) &&
					 ($code > $rmax{$lib}) ) {
					$rmax{$lib} = $code;
				}
			} else {
				if ($fassigned{$lib} =~ /:$code:/) {
					print STDERR "!! ERROR: $lib function code $code assigned twice (collision at $name)\n";
					++$errcount;
				}
				$fassigned{$lib} .= "$code:";
				if($code > $fmax{$lib}) {
					$fmax{$lib} = $code;
				}
				$fcodes{$name} = $code;
			}
		}
	  }
	}

	if ($debug) {
		if (defined($fmax{$lib})) {
			print STDERR "Max function code fmax" . "{" . "$lib" . "} = $fmax{$lib}\n";
			$fassigned{$lib} =~ m/^:(.*):$/;
			@fassigned = sort {$a <=> $b} split(":", $1);
			print STDERR "  @fassigned\n";
		}
		if (defined($rmax{$lib})) {
			print STDERR "Max reason code rmax" . "{" . "$lib" . "} = $rmax{$lib}\n";
			$rassigned{$lib} =~ m/^:(.*):$/;
			@rassigned = sort {$a <=> $b} split(":", $1);
			print STDERR "  @rassigned\n";
		}
	}

	if ($lib eq "SSL") {
		if ($rmax{$lib} >= 1000) {
			print STDERR "!! ERROR: SSL error codes 1000+ are reserved for alerts.\n";
			print STDERR "!!        Any new alerts must be added to $config.\n";
			++$errcount;
			print STDERR "\n";
		}
	}
	close IN;
}

# Scan each C source file and look for function and reason codes
# This is done by looking for strings that "look like" function or
# reason codes: basically anything consisting of all upper case and
# numerics which has _F_ or _R_ in it and which has the name of an
# error library at the start. This seems to work fine except for the
# oddly named structure BIO_F_CTX which needs to be ignored.
# If a code doesn't exist in list compiled from headers then mark it
# with the value "X" as a place holder to give it a value later.
# Store all function and reason codes found in %ufcodes and %urcodes
# so all those unreferenced can be printed out.


foreach $file (@source) {
	# Don't parse the error source file.
	next if exists $cskip{$file};
	print STDERR "File loaded: ".$file."\r" if $debug;
	open(IN, "<$file") || die "Can't open source file $file\n";
	while(<IN>) {
		# skip obsoleted source files entirely!
		last if(/^#error\s+obsolete/);

		if(/(([A-Z0-9]+)_F_([A-Z0-9_]+))/) {
			next unless exists $csrc{$2};
			next if($1 eq "BIO_F_BUFFER_CTX");
			$ufcodes{$1} = 1;
			if(!exists $fcodes{$1}) {
				$fcodes{$1} = "X";
				$fnew{$2}++;
			}
			$notrans{$1} = 1 unless exists $ftrans{$3};
			print STDERR "Function: $1\t= $fcodes{$1} (lib: $2, name: $3)\n" if $debug; 
		}
		if(/(([A-Z0-9]+)_R_[A-Z0-9_]+)/) {
			next unless exists $csrc{$2};
			$urcodes{$1} = 1;
			if(!exists $rcodes{$1}) {
				$rcodes{$1} = "X";
				$rnew{$2}++;
			}
			print STDERR "Reason: $1\t= $rcodes{$1} (lib: $2)\n" if $debug; 
		} 
	}
	close IN;
}
print STDERR "                                  \n" if $debug;

# Now process each library in turn.

foreach $lib (keys %csrc)
{
	my $hfile = $hinc{$lib};
	my $cfile = $csrc{$lib};
	if(!$fnew{$lib} && !$rnew{$lib}) {
		print STDERR "$lib:\t\tNo new error codes\n";
		next unless $rebuild;
	} else {
		print STDERR "$lib:\t\t$fnew{$lib} New Functions,";
		print STDERR " $rnew{$lib} New Reasons.\n";
		next unless $dowrite;
	}

	# If we get here then we have some new error codes so we
	# need to rebuild the header file and C file.

	# Make a sorted list of error and reason codes for later use.

	my @function = sort grep(/^${lib}_/,keys %fcodes);
	my @reasons = sort grep(/^${lib}_/,keys %rcodes);

	# Rewrite the header file

	if (open(IN, "<$hfile")) {
	    # Copy across the old file
	    while(<IN>) {
		push @out, $_;
		last if (/BEGIN ERROR CODES/);
	    }
	    close IN;
	} else {
	    push @out,
"/* ====================================================================\n",
" * Copyright (c) 2001-2011 The OpenSSL Project.  All rights reserved.\n",
" *\n",
" * Redistribution and use in source and binary forms, with or without\n",
" * modification, are permitted provided that the following conditions\n",
" * are met:\n",
" *\n",
" * 1. Redistributions of source code must retain the above copyright\n",
" *    notice, this list of conditions and the following disclaimer. \n",
" *\n",
" * 2. Redistributions in binary form must reproduce the above copyright\n",
" *    notice, this list of conditions and the following disclaimer in\n",
" *    the documentation and/or other materials provided with the\n",
" *    distribution.\n",
" *\n",
" * 3. All advertising materials mentioning features or use of this\n",
" *    software must display the following acknowledgment:\n",
" *    \"This product includes software developed by the OpenSSL Project\n",
" *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)\"\n",
" *\n",
" * 4. The names \"OpenSSL Toolkit\" and \"OpenSSL Project\" must not be used to\n",
" *    endorse or promote products derived from this software without\n",
" *    prior written permission. For written permission, please contact\n",
" *    openssl-core\@openssl.org.\n",
" *\n",
" * 5. Products derived from this software may not be called \"OpenSSL\"\n",
" *    nor may \"OpenSSL\" appear in their names without prior written\n",
" *    permission of the OpenSSL Project.\n",
" *\n",
" * 6. Redistributions of any form whatsoever must retain the following\n",
" *    acknowledgment:\n",
" *    \"This product includes software developed by the OpenSSL Project\n",
" *    for use in the OpenSSL Toolkit (http://www.openssl.org/)\"\n",
" *\n",
" * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY\n",
" * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n",
" * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n",
" * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR\n",
" * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n",
" * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT\n",
" * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n",
" * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n",
" * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,\n",
" * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)\n",
" * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED\n",
" * OF THE POSSIBILITY OF SUCH DAMAGE.\n",
" * ====================================================================\n",
" *\n",
" * This product includes cryptographic software written by Eric Young\n",
" * (eay\@cryptsoft.com).  This product includes software written by Tim\n",
" * Hudson (tjh\@cryptsoft.com).\n",
" *\n",
" */\n",
"\n",
"#ifndef HEADER_${lib}_ERR_H\n",
"#define HEADER_${lib}_ERR_H\n",
"\n",
"#ifdef  __cplusplus\n",
"extern \"C\" {\n",
"#endif\n",
"\n",
"/* BEGIN ERROR CODES */\n";
	}
	open (OUT, ">$hfile") || die "Can't Open File $hfile for writing\n";

	print OUT @out;
	undef @out;
	print OUT <<"EOF";
/* The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
EOF
	if($static) {
		print OUT <<"EOF";
${staticloader}void ERR_load_${lib}_strings(void);

EOF
	} else {
		print OUT <<"EOF";
${staticloader}void ERR_load_${lib}_strings(void);
${staticloader}void ERR_unload_${lib}_strings(void);
${staticloader}void ERR_${lib}_error(int function, int reason, char *file, int line);
#define ${lib}err(f,r) ERR_${lib}_error((f),(r),__FILE__,__LINE__)

EOF
	}
	print OUT <<"EOF";
/* Error codes for the $lib functions. */

/* Function codes. */
EOF

	foreach $i (@function) {
		$z=6-int(length($i)/8);
		if($fcodes{$i} eq "X") {
			$fassigned{$lib} =~ m/^:([^:]*):/;
			$findcode = $1;
			if (!defined($findcode)) {
				$findcode = $fmax{$lib};
			}
			while ($fassigned{$lib} =~ m/:$findcode:/) {
				$findcode++;
			}
			$fcodes{$i} = $findcode;
			$fassigned{$lib} .= "$findcode:";
			print STDERR "New Function code $i\n" if $debug;
		}
		printf OUT "#define $i%s $fcodes{$i}\n","\t" x $z;
	}

	print OUT "\n/* Reason codes. */\n";

	foreach $i (@reasons) {
		$z=6-int(length($i)/8);
		if($rcodes{$i} eq "X") {
			$rassigned{$lib} =~ m/^:([^:]*):/;
			$findcode = $1;
			if (!defined($findcode)) {
				$findcode = $rmax{$lib};
			}
			while ($rassigned{$lib} =~ m/:$findcode:/) {
				$findcode++;
			}
			$rcodes{$i} = $findcode;
			$rassigned{$lib} .= "$findcode:";
			print STDERR "New Reason code   $i\n" if $debug;
		}
		printf OUT "#define $i%s $rcodes{$i}\n","\t" x $z;
	}
	print OUT <<"EOF";

#ifdef  __cplusplus
}
#endif
#endif
EOF
	close OUT;

	# Rewrite the C source file containing the error details.

	# First, read any existing reason string definitions:
	my %err_reason_strings;
	if (open(IN,"<$cfile")) {
		while (<IN>) {
			if (/\b(${lib}_R_\w*)\b.*\"(.*)\"/) {
				$err_reason_strings{$1} = $2;
			}
			if (/\b${lib}_F_(\w*)\b.*\"(.*)\"/) {
				if (!exists $ftrans{$1} && ($1 ne $2)) {
					print STDERR "WARNING: Mismatched function string $2\n";
					$ftrans{$1} = $2;
				}
			}
		}
		close(IN);
	}


	my $hincf;
	if($static) {
		$hfile =~ /([^\/]+)$/;
		$hincf = "<${hprefix}$1>";
	} else {
		$hincf = "\"$hfile\"";
	}

	# If static we know the error code at compile time so use it
	# in error definitions.

	if ($static)
		{
		$pack_errcode = "ERR_LIB_${lib}";
		$load_errcode = "0";
		}
	else
		{
		$pack_errcode = "0";
		$load_errcode = "ERR_LIB_${lib}";
		}


	open (OUT,">$cfile") || die "Can't open $cfile for writing";

	print OUT <<"EOF";
/* $cfile */
/* ====================================================================
 * Copyright (c) 1999-2011 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core\@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay\@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh\@cryptsoft.com).
 *
 */

/* NOTE: this file was auto generated by the mkerr.pl script: any changes
 * made to it will be overwritten when the script next updates this file,
 * only reason strings will be preserved.
 */

#include <stdio.h>
#include <openssl/err.h>
#include $hincf

/* BEGIN ERROR CODES */
#ifndef OPENSSL_NO_ERR

#define ERR_FUNC(func) ERR_PACK($pack_errcode,func,0)
#define ERR_REASON(reason) ERR_PACK($pack_errcode,0,reason)

static ERR_STRING_DATA ${lib}_str_functs[]=
	{
EOF
	# Add each function code: if a function name is found then use it.
	foreach $i (@function) {
		my $fn;
		$i =~ /^${lib}_F_(\S+)$/;
		$fn = $1;
		if(exists $ftrans{$fn}) {
			$fn = $ftrans{$fn};
		}
#		print OUT "{ERR_PACK($pack_errcode,$i,0),\t\"$fn\"},\n";
		print OUT "{ERR_FUNC($i),\t\"$fn\"},\n";
	}
	print OUT <<"EOF";
{0,NULL}
	};

static ERR_STRING_DATA ${lib}_str_reasons[]=
	{
EOF
	# Add each reason code.
	foreach $i (@reasons) {
		my $rn;
		my $rstr = "ERR_REASON($i)";
		my $nspc = 0;
		if (exists $err_reason_strings{$i}) {
			$rn = $err_reason_strings{$i};
		} else {
			$i =~ /^${lib}_R_(\S+)$/;
			$rn = $1;
			$rn =~ tr/_[A-Z]/ [a-z]/;
		}
		$nspc = 40 - length($rstr) unless length($rstr) > 40;
		$nspc = " " x $nspc;
		print OUT "{${rstr}${nspc},\"$rn\"},\n";
	}
if($static) {
	print OUT <<"EOF";
{0,NULL}
	};

#endif

${staticloader}void ERR_load_${lib}_strings(void)
	{
#ifndef OPENSSL_NO_ERR

	if (ERR_func_error_string(${lib}_str_functs[0].error) == NULL)
		{
		ERR_load_strings($load_errcode,${lib}_str_functs);
		ERR_load_strings($load_errcode,${lib}_str_reasons);
		}
#endif
	}
EOF
} else {
	print OUT <<"EOF";
{0,NULL}
	};

#endif

#ifdef ${lib}_LIB_NAME
static ERR_STRING_DATA ${lib}_lib_name[]=
        {
{0	,${lib}_LIB_NAME},
{0,NULL}
	};
#endif


static int ${lib}_lib_error_code=0;
static int ${lib}_error_init=1;

${staticloader}void ERR_load_${lib}_strings(void)
	{
	if (${lib}_lib_error_code == 0)
		${lib}_lib_error_code=ERR_get_next_error_library();

	if (${lib}_error_init)
		{
		${lib}_error_init=0;
#ifndef OPENSSL_NO_ERR
		ERR_load_strings(${lib}_lib_error_code,${lib}_str_functs);
		ERR_load_strings(${lib}_lib_error_code,${lib}_str_reasons);
#endif

#ifdef ${lib}_LIB_NAME
		${lib}_lib_name->error = ERR_PACK(${lib}_lib_error_code,0,0);
		ERR_load_strings(0,${lib}_lib_name);
#endif
		}
	}

${staticloader}void ERR_unload_${lib}_strings(void)
	{
	if (${lib}_error_init == 0)
		{
#ifndef OPENSSL_NO_ERR
		ERR_unload_strings(${lib}_lib_error_code,${lib}_str_functs);
		ERR_unload_strings(${lib}_lib_error_code,${lib}_str_reasons);
#endif

#ifdef ${lib}_LIB_NAME
		ERR_unload_strings(0,${lib}_lib_name);
#endif
		${lib}_error_init=1;
		}
	}

${staticloader}void ERR_${lib}_error(int function, int reason, char *file, int line)
	{
	if (${lib}_lib_error_code == 0)
		${lib}_lib_error_code=ERR_get_next_error_library();
	ERR_PUT_error(${lib}_lib_error_code,function,reason,file,line);
	}
EOF

}

	close OUT;
	undef %err_reason_strings;
}

if($debug && %notrans) {
	print STDERR "The following function codes were not translated:\n";
	foreach(sort keys %notrans)
	{
		print STDERR "$_\n";
	}
}

# Make a list of unreferenced function and reason codes

foreach (keys %fcodes) {
	push (@funref, $_) unless exists $ufcodes{$_};
}

foreach (keys %rcodes) {
	push (@runref, $_) unless exists $urcodes{$_};
}

if($debug && @funref) {
	print STDERR "The following function codes were not referenced:\n";
	foreach(sort @funref)
	{
		print STDERR "$_\n";
	}
}

if($debug && @runref) {
	print STDERR "The following reason codes were not referenced:\n";
	foreach(sort @runref)
	{
		print STDERR "$_\n";
	}
}

if($errcount) {
	print STDERR "There were errors, failing...\n\n";
	exit $errcount;
}

