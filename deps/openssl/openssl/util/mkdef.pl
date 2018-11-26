#! /usr/bin/env perl
# Copyright 1995-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

#
# generate a .def file
#
# It does this by parsing the header files and looking for the
# prototyped functions: it then prunes the output.
#
# Intermediary files are created, call libcrypto.num and libssl.num,
# The format of these files is:
#
#	routine-name	nnnn	vers	info
#
# The "nnnn" and "vers" fields are the numeric id and version for the symbol
# respectively. The "info" part is actually a colon-separated string of fields
# with the following meaning:
#
#	existence:platform:kind:algorithms
#
# - "existence" can be "EXIST" or "NOEXIST" depending on if the symbol is
#   found somewhere in the source, 
# - "platforms" is empty if it exists on all platforms, otherwise it contains
#   comma-separated list of the platform, just as they are if the symbol exists
#   for those platforms, or prepended with a "!" if not.  This helps resolve
#   symbol name variants for platforms where the names are too long for the
#   compiler or linker, or if the systems is case insensitive and there is a
#   clash, or the symbol is implemented differently (see
#   EXPORT_VAR_AS_FUNCTION).  This script assumes renaming of symbols is found
#   in the file crypto/symhacks.h.
#   The semantics for the platforms is that every item is checked against the
#   environment.  For the negative items ("!FOO"), if any of them is false
#   (i.e. "FOO" is true) in the environment, the corresponding symbol can't be
#   used.  For the positive itms, if all of them are false in the environment,
#   the corresponding symbol can't be used.  Any combination of positive and
#   negative items are possible, and of course leave room for some redundancy.
# - "kind" is "FUNCTION" or "VARIABLE".  The meaning of that is obvious.
# - "algorithms" is a comma-separated list of algorithm names.  This helps
#   exclude symbols that are part of an algorithm that some user wants to
#   exclude.
#

use lib ".";
use configdata;
use File::Spec::Functions;
use File::Basename;
use FindBin;
use lib "$FindBin::Bin/perl";
use OpenSSL::Glob;

# When building a "variant" shared library, with a custom SONAME, also customize
# all the symbol versions.  This produces a shared object that can coexist
# without conflict in the same address space as a default build, or an object
# with a different variant tag.
#
# For example, with a target definition that includes:
#
#         shlib_variant => "-opt",
#
# we build the following objects:
#
# $ perl -le '
#     for (@ARGV) {
#         if ($l = readlink) {
#             printf "%s -> %s\n", $_, $l
#         } else {
#             print
#         }
#     }' *.so*
# libcrypto-opt.so.1.1
# libcrypto.so -> libcrypto-opt.so.1.1
# libssl-opt.so.1.1
# libssl.so -> libssl-opt.so.1.1
#
# whose SONAMEs and dependencies are:
#
# $ for l in *.so; do
#     echo $l
#     readelf -d $l | egrep 'SONAME|NEEDED.*(ssl|crypto)'
#   done
# libcrypto.so
#  0x000000000000000e (SONAME)             Library soname: [libcrypto-opt.so.1.1]
# libssl.so
#  0x0000000000000001 (NEEDED)             Shared library: [libcrypto-opt.so.1.1]
#  0x000000000000000e (SONAME)             Library soname: [libssl-opt.so.1.1]
#
# We case-fold the variant tag to upper case and replace all non-alnum
# characters with "_".  This yields the following symbol versions:
#
# $ nm libcrypto.so | grep -w A
# 0000000000000000 A OPENSSL_OPT_1_1_0
# 0000000000000000 A OPENSSL_OPT_1_1_0a
# 0000000000000000 A OPENSSL_OPT_1_1_0c
# 0000000000000000 A OPENSSL_OPT_1_1_0d
# 0000000000000000 A OPENSSL_OPT_1_1_0f
# 0000000000000000 A OPENSSL_OPT_1_1_0g
# $ nm libssl.so | grep -w A
# 0000000000000000 A OPENSSL_OPT_1_1_0
# 0000000000000000 A OPENSSL_OPT_1_1_0d
#
(my $SO_VARIANT = qq{\U$target{"shlib_variant"}}) =~ s/\W/_/g;

my $debug=0;
my $trace=0;
my $verbose=0;

my $crypto_num= catfile($config{sourcedir},"util","libcrypto.num");
my $ssl_num=    catfile($config{sourcedir},"util","libssl.num");
my $libname;

my $do_update = 0;
my $do_rewrite = 1;
my $do_crypto = 0;
my $do_ssl = 0;
my $do_ctest = 0;
my $do_ctestall = 0;
my $do_checkexist = 0;

my $VMS=0;
my $W32=0;
my $NT=0;
my $UNIX=0;
my $linux=0;
# Set this to make typesafe STACK definitions appear in DEF
my $safe_stack_def = 0;

my @known_platforms = ( "__FreeBSD__", "PERL5",
			"EXPORT_VAR_AS_FUNCTION", "ZLIB", "_WIN32"
			);
my @known_ossl_platforms = ( "UNIX", "VMS", "WIN32", "WINNT", "OS2" );
my @known_algorithms = ( # These are algorithms we know are guarded in relevant
			 # header files, but aren't actually disablable.
			 # Without these, this script will warn a lot.
			 "RSA", "MD5",
			 # @disablables comes from configdata.pm
			 map { (my $x = uc $_) =~ s|-|_|g; $x; } @disablables,
			 # Deprecated functions.  Not really algorithmss, but
			 # treated as such here for the sake of simplicity
			 "DEPRECATEDIN_0_9_8",
			 "DEPRECATEDIN_1_0_0",
			 "DEPRECATEDIN_1_1_0",
                     );

# %disabled comes from configdata.pm
my %disabled_algorithms =
    map { (my $x = uc $_) =~ s|-|_|g; $x => 1; } keys %disabled;

my $zlib;

foreach (@ARGV, split(/ /, $config{options}))
	{
	$debug=1 if $_ eq "debug";
	$trace=1 if $_ eq "trace";
	$verbose=1 if $_ eq "verbose";
	$W32=1 if $_ eq "32";
	die "win16 not supported" if $_ eq "16";
	if($_ eq "NT") {
		$W32 = 1;
		$NT = 1;
	}
	if ($_ eq "linux") {
		$linux=1;
		$UNIX=1;
	}
	$VMS=1 if $_ eq "VMS";
	if ($_ eq "zlib" || $_ eq "enable-zlib" || $_ eq "zlib-dynamic"
			 || $_ eq "enable-zlib-dynamic") {
		$zlib = 1;
	}

	$do_ssl=1 if $_ eq "libssl";
	if ($_ eq "ssl") {
		$do_ssl=1; 
		$libname=$_
	}
	$do_crypto=1 if $_ eq "libcrypto";
	if ($_ eq "crypto") {
		$do_crypto=1;
		$libname=$_;
	}
	$do_update=1 if $_ eq "update";
	$do_rewrite=1 if $_ eq "rewrite";
	$do_ctest=1 if $_ eq "ctest";
	$do_ctestall=1 if $_ eq "ctestall";
	$do_checkexist=1 if $_ eq "exist";
	if (/^--api=(\d+)\.(\d+)\.(\d+)$/) {
		my $apiv = sprintf "%x%02x%02x", $1, $2, $3;
		foreach (@known_algorithms) {
			if (/^DEPRECATEDIN_(\d+)_(\d+)_(\d+)$/) {
				my $depv = sprintf "%x%02x%02x", $1, $2, $3;
				$disabled_algorithms{$_} = 1 if $apiv ge $depv;
			}
		}
	}
	if (/^no-deprecated$/) {
		foreach (@known_algorithms) {
			if (/^DEPRECATEDIN_/) {
				$disabled_algorithms{$_} = 1;
			}
		}
	}
	elsif (/^(enable|disable|no)-(.*)$/) {
		my $alg = uc $2;
        $alg =~ tr/-/_/;
		if (exists $disabled_algorithms{$alg}) {
			$disabled_algorithms{$alg} = $1 eq "enable" ? 0 : 1;
		}
	}

	}

if (!$libname) { 
	if ($do_ssl) {
		$libname="LIBSSL";
	}
	if ($do_crypto) {
		$libname="LIBCRYPTO";
	}
}

# If no platform is given, assume WIN32
if ($W32 + $VMS + $linux == 0) {
	$W32 = 1;
}
die "Please, only one platform at a time"
    if ($W32 + $VMS + $linux > 1);

if (!$do_ssl && !$do_crypto)
	{
	print STDERR "usage: $0 ( ssl | crypto ) [ 16 | 32 | NT | OS2 | linux | VMS ]\n";
	exit(1);
	}

%ssl_list=&load_numbers($ssl_num);
$max_ssl = $max_num;
%crypto_list=&load_numbers($crypto_num);
$max_crypto = $max_num;

my $ssl="include/openssl/ssl.h";
$ssl.=" include/openssl/tls1.h";
$ssl.=" include/openssl/srtp.h";

# We use headers found in include/openssl and include/internal only.
# The latter is needed so libssl.so/.dll/.exe can link properly.
my $crypto ="include/openssl/crypto.h";
$crypto.=" include/internal/o_dir.h";
$crypto.=" include/internal/o_str.h";
$crypto.=" include/internal/err.h";
$crypto.=" include/internal/asn1t.h";
$crypto.=" include/internal/sslconf.h";
$crypto.=" include/openssl/des.h" ; # unless $no_des;
$crypto.=" include/openssl/idea.h" ; # unless $no_idea;
$crypto.=" include/openssl/rc4.h" ; # unless $no_rc4;
$crypto.=" include/openssl/rc5.h" ; # unless $no_rc5;
$crypto.=" include/openssl/rc2.h" ; # unless $no_rc2;
$crypto.=" include/openssl/blowfish.h" ; # unless $no_bf;
$crypto.=" include/openssl/cast.h" ; # unless $no_cast;
$crypto.=" include/openssl/whrlpool.h" ;
$crypto.=" include/openssl/md2.h" ; # unless $no_md2;
$crypto.=" include/openssl/md4.h" ; # unless $no_md4;
$crypto.=" include/openssl/md5.h" ; # unless $no_md5;
$crypto.=" include/openssl/mdc2.h" ; # unless $no_mdc2;
$crypto.=" include/openssl/sha.h" ; # unless $no_sha;
$crypto.=" include/openssl/ripemd.h" ; # unless $no_ripemd;
$crypto.=" include/openssl/aes.h" ; # unless $no_aes;
$crypto.=" include/openssl/camellia.h" ; # unless $no_camellia;
$crypto.=" include/openssl/seed.h"; # unless $no_seed;

$crypto.=" include/openssl/bn.h";
$crypto.=" include/openssl/rsa.h" ; # unless $no_rsa;
$crypto.=" include/openssl/dsa.h" ; # unless $no_dsa;
$crypto.=" include/openssl/dh.h" ; # unless $no_dh;
$crypto.=" include/openssl/ec.h" ; # unless $no_ec;
$crypto.=" include/openssl/hmac.h" ; # unless $no_hmac;
$crypto.=" include/openssl/cmac.h" ;

$crypto.=" include/openssl/engine.h"; # unless $no_engine;
$crypto.=" include/openssl/stack.h" ; # unless $no_stack;
$crypto.=" include/openssl/buffer.h" ; # unless $no_buffer;
$crypto.=" include/openssl/bio.h" ; # unless $no_bio;
$crypto.=" include/internal/dso.h" ; # unless $no_dso;
$crypto.=" include/openssl/lhash.h" ; # unless $no_lhash;
$crypto.=" include/openssl/conf.h";
$crypto.=" include/openssl/txt_db.h";

$crypto.=" include/openssl/evp.h" ; # unless $no_evp;
$crypto.=" include/openssl/objects.h";
$crypto.=" include/openssl/pem.h";
#$crypto.=" include/openssl/meth.h";
$crypto.=" include/openssl/asn1.h";
$crypto.=" include/openssl/asn1t.h";
$crypto.=" include/openssl/err.h" ; # unless $no_err;
$crypto.=" include/openssl/pkcs7.h";
$crypto.=" include/openssl/pkcs12.h";
$crypto.=" include/openssl/x509.h";
$crypto.=" include/openssl/x509_vfy.h";
$crypto.=" include/openssl/x509v3.h";
$crypto.=" include/openssl/ts.h";
$crypto.=" include/openssl/rand.h";
$crypto.=" include/openssl/comp.h" ; # unless $no_comp;
$crypto.=" include/openssl/ocsp.h";
$crypto.=" include/openssl/ui.h";
#$crypto.=" include/openssl/store.h";
$crypto.=" include/openssl/cms.h";
$crypto.=" include/openssl/srp.h";
$crypto.=" include/openssl/modes.h";
$crypto.=" include/openssl/async.h";
$crypto.=" include/openssl/ct.h";
$crypto.=" include/openssl/kdf.h";

my $symhacks="include/openssl/symhacks.h";

my @ssl_symbols = &do_defs("LIBSSL", $ssl, $symhacks);
my @crypto_symbols = &do_defs("LIBCRYPTO", $crypto, $symhacks);

if ($do_update) {

if ($do_ssl == 1) {

	&maybe_add_info("LIBSSL",*ssl_list,@ssl_symbols);
	if ($do_rewrite == 1) {
		open(OUT, ">$ssl_num");
		&rewrite_numbers(*OUT,"LIBSSL",*ssl_list,@ssl_symbols);
	} else {
		open(OUT, ">>$ssl_num");
	}
	&update_numbers(*OUT,"LIBSSL",*ssl_list,$max_ssl,@ssl_symbols);
	close OUT;
}

if($do_crypto == 1) {

	&maybe_add_info("LIBCRYPTO",*crypto_list,@crypto_symbols);
	if ($do_rewrite == 1) {
		open(OUT, ">$crypto_num");
		&rewrite_numbers(*OUT,"LIBCRYPTO",*crypto_list,@crypto_symbols);
	} else {
		open(OUT, ">>$crypto_num");
	}
	&update_numbers(*OUT,"LIBCRYPTO",*crypto_list,$max_crypto,@crypto_symbols);
	close OUT;
} 

} elsif ($do_checkexist) {
	&check_existing(*ssl_list, @ssl_symbols)
		if $do_ssl == 1;
	&check_existing(*crypto_list, @crypto_symbols)
		if $do_crypto == 1;
} elsif ($do_ctest || $do_ctestall) {

	print <<"EOF";

/* Test file to check all DEF file symbols are present by trying
 * to link to all of them. This is *not* intended to be run!
 */

int main()
{
EOF
	&print_test_file(*STDOUT,"LIBSSL",*ssl_list,$do_ctestall,@ssl_symbols)
		if $do_ssl == 1;

	&print_test_file(*STDOUT,"LIBCRYPTO",*crypto_list,$do_ctestall,@crypto_symbols)
		if $do_crypto == 1;

	print "}\n";

} else {

	&print_def_file(*STDOUT,$libname,*ssl_list,@ssl_symbols)
		if $do_ssl == 1;

	&print_def_file(*STDOUT,$libname,*crypto_list,@crypto_symbols)
		if $do_crypto == 1;

}


sub do_defs
{
	my($name,$files,$symhacksfile)=@_;
	my $file;
	my @ret;
	my %syms;
	my %platform;		# For anything undefined, we assume ""
	my %kind;		# For anything undefined, we assume "FUNCTION"
	my %algorithm;		# For anything undefined, we assume ""
	my %variant;
	my %variant_cnt;	# To be able to allocate "name{n}" if "name"
				# is the same name as the original.
	my $cpp;
	my %unknown_algorithms = ();
	my $parens = 0;

	foreach $file (split(/\s+/,$symhacksfile." ".$files))
		{
		my $fn = catfile($config{sourcedir},$file);
		print STDERR "TRACE: start reading $fn\n" if $trace;
		open(IN,"<$fn") || die "unable to open $fn:$!\n";
		my $line = "", my $def= "";
		my %tag = (
			(map { $_ => 0 } @known_platforms),
			(map { "OPENSSL_SYS_".$_ => 0 } @known_ossl_platforms),
			(map { "OPENSSL_NO_".$_ => 0 } @known_algorithms),
			(map { "OPENSSL_USE_".$_ => 0 } @known_algorithms),
			NOPROTO		=> 0,
			PERL5		=> 0,
			_WINDLL		=> 0,
			CONST_STRICT	=> 0,
			TRUE		=> 1,
		);
		my $symhacking = $file eq $symhacksfile;
		my @current_platforms = ();
		my @current_algorithms = ();

		# params: symbol, alias, platforms, kind
		# The reason to put this subroutine in a variable is that
		# it will otherwise create it's own, unshared, version of
		# %tag and %variant...
		my $make_variant = sub
		{
			my ($s, $a, $p, $k) = @_;
			my ($a1, $a2);

			print STDERR "DEBUG: make_variant: Entered with ",$s,", ",$a,", ",(defined($p)?$p:""),", ",(defined($k)?$k:""),"\n" if $debug;
			if (defined($p))
			{
				$a1 = join(",",$p,
					   grep(!/^$/,
						map { $tag{$_} == 1 ? $_ : "" }
						@known_platforms));
			}
			else
			{
				$a1 = join(",",
					   grep(!/^$/,
						map { $tag{$_} == 1 ? $_ : "" }
						@known_platforms));
			}
			$a2 = join(",",
				   grep(!/^$/,
					map { $tag{"OPENSSL_SYS_".$_} == 1 ? $_ : "" }
					@known_ossl_platforms));
			print STDERR "DEBUG: make_variant: a1 = $a1; a2 = $a2\n" if $debug;
			if ($a1 eq "") { $a1 = $a2; }
			elsif ($a1 ne "" && $a2 ne "") { $a1 .= ",".$a2; }
			if ($a eq $s)
			{
				if (!defined($variant_cnt{$s}))
				{
					$variant_cnt{$s} = 0;
				}
				$variant_cnt{$s}++;
				$a .= "{$variant_cnt{$s}}";
			}
			my $toadd = $a.":".$a1.(defined($k)?":".$k:"");
			my $togrep = $s.'(\{[0-9]+\})?:'.$a1.(defined($k)?":".$k:"");
			if (!grep(/^$togrep$/,
				  split(/;/, defined($variant{$s})?$variant{$s}:""))) {
				if (defined($variant{$s})) { $variant{$s} .= ";"; }
				$variant{$s} .= $toadd;
			}
			print STDERR "DEBUG: make_variant: Exit with variant of ",$s," = ",$variant{$s},"\n" if $debug;
		};

		print STDERR "DEBUG: parsing ----------\n" if $debug;
		while(<IN>) {
			s|\R$||; # Better chomp
			if($parens > 0) {
				#Inside a DEPRECATEDIN
				$stored_multiline .= $_;
				print STDERR "DEBUG: Continuing multiline DEPRECATEDIN: $stored_multiline\n" if $debug;
				$parens = count_parens($stored_multiline);
				if ($parens == 0) {
					$def .= do_deprecated($stored_multiline,
							\@current_platforms,
							\@current_algorithms);
				}
				next;
			}
			if (/\/\* Error codes for the \w+ functions\. \*\//)
				{
				undef @tag;
				last;
				}
			if ($line ne '') {
				$_ = $line . $_;
				$line = '';
			}

			if (/\\$/) {
				$line = $`; # keep what was before the backslash
				next;
			}

			if(/\/\*/) {
				if (not /\*\//) {	# multiline comment...
					$line = $_;	# ... just accumulate
					next;
				} else {
					s/\/\*.*?\*\///gs;# wipe it
				}
			}

			if ($cpp) {
				$cpp++ if /^#\s*if/;
				$cpp-- if /^#\s*endif/;
				next;
			}
			if (/^#.*ifdef.*cplusplus/) {
				$cpp = 1;
				next;
			}

			s/{[^{}]*}//gs;                      # ignore {} blocks
			print STDERR "DEBUG: \$def=\"$def\"\n" if $debug && $def ne "";
			print STDERR "DEBUG: \$_=\"$_\"\n" if $debug;
			if (/^\#\s*ifndef\s+(.*)/) {
				push(@tag,"-");
				push(@tag,$1);
				$tag{$1}=-1;
				print STDERR "DEBUG: $file: found tag $1 = -1\n" if $debug;
			} elsif (/^\#\s*if\s+!defined\s*\(([^\)]+)\)/) {
				push(@tag,"-");
				if (/^\#\s*if\s+(!defined\s*\(([^\)]+)\)(\s+\&\&\s+!defined\s*\(([^\)]+)\))*)$/) {
					my $tmp_1 = $1;
					my $tmp_;
					foreach $tmp_ (split '\&\&',$tmp_1) {
						$tmp_ =~ /!defined\s*\(([^\)]+)\)/;
						print STDERR "DEBUG: $file: found tag $1 = -1\n" if $debug;
						push(@tag,$1);
						$tag{$1}=-1;
					}
				} else {
					print STDERR "Warning: $file: taking only '!defined($1)' of complicated expression: $_" if $verbose; # because it is O...
					print STDERR "DEBUG: $file: found tag $1 = -1\n" if $debug;
					push(@tag,$1);
					$tag{$1}=-1;
				}
			} elsif (/^\#\s*ifdef\s+(\S*)/) {
				push(@tag,"-");
				push(@tag,$1);
				$tag{$1}=1;
				print STDERR "DEBUG: $file: found tag $1 = 1\n" if $debug;
			} elsif (/^\#\s*if\s+defined\s*\(([^\)]+)\)/) {
				push(@tag,"-");
				if (/^\#\s*if\s+(defined\s*\(([^\)]+)\)(\s+\|\|\s+defined\s*\(([^\)]+)\))*)$/) {
					my $tmp_1 = $1;
					my $tmp_;
					foreach $tmp_ (split '\|\|',$tmp_1) {
						$tmp_ =~ /defined\s*\(([^\)]+)\)/;
						print STDERR "DEBUG: $file: found tag $1 = 1\n" if $debug;
						push(@tag,$1);
						$tag{$1}=1;
					}
				} else {
					print STDERR "Warning: $file: taking only 'defined($1)' of complicated expression: $_\n" if $verbose; # because it is O...
					print STDERR "DEBUG: $file: found tag $1 = 1\n" if $debug;
					push(@tag,$1);
					$tag{$1}=1;
				}
			} elsif (/^\#\s*error\s+(\w+) is disabled\./) {
				my $tag_i = $#tag;
				while($tag[$tag_i] ne "-") {
					if ($tag[$tag_i] eq "OPENSSL_NO_".$1) {
						$tag{$tag[$tag_i]}=2;
						print STDERR "DEBUG: $file: chaged tag $1 = 2\n" if $debug;
					}
					$tag_i--;
				}
			} elsif (/^\#\s*endif/) {
				my $tag_i = $#tag;
				while($tag_i > 0 && $tag[$tag_i] ne "-") {
					my $t=$tag[$tag_i];
					print STDERR "DEBUG: \$t=\"$t\"\n" if $debug;
					if ($tag{$t}==2) {
						$tag{$t}=-1;
					} else {
						$tag{$t}=0;
					}
					print STDERR "DEBUG: $file: changed tag ",$t," = ",$tag{$t},"\n" if $debug;
					pop(@tag);
					if ($t =~ /^OPENSSL_NO_([A-Z0-9_]+)$/) {
						$t=$1;
					} elsif($t =~ /^OPENSSL_USE_([A-Z0-9_]+)$/) {
						$t=$1;
					} else {
						$t="";
					}
					if ($t ne ""
					    && !grep(/^$t$/, @known_algorithms)) {
						$unknown_algorithms{$t} = 1;
						#print STDERR "DEBUG: Added as unknown algorithm: $t\n" if $debug;
					}
					$tag_i--;
				}
				pop(@tag);
			} elsif (/^\#\s*else/) {
				my $tag_i = $#tag;
				die "$file unmatched else\n" if $tag_i < 0;
				while($tag[$tag_i] ne "-") {
					my $t=$tag[$tag_i];
					$tag{$t}= -$tag{$t};
					print STDERR "DEBUG: $file: changed tag ",$t," = ",$tag{$t},"\n" if $debug;
					$tag_i--;
				}
			} elsif (/^\#\s*if\s+1/) {
				push(@tag,"-");
				# Dummy tag
				push(@tag,"TRUE");
				$tag{"TRUE"}=1;
				print STDERR "DEBUG: $file: found 1\n" if $debug;
			} elsif (/^\#\s*if\s+0/) {
				push(@tag,"-");
				# Dummy tag
				push(@tag,"TRUE");
				$tag{"TRUE"}=-1;
				print STDERR "DEBUG: $file: found 0\n" if $debug;
			} elsif (/^\#\s*if\s+/) {
				#Some other unrecognized "if" style
				push(@tag,"-");
				print STDERR "Warning: $file: ignoring unrecognized expression: $_\n" if $verbose; # because it is O...
			} elsif (/^\#\s*define\s+(\w+)\s+(\w+)/
				 && $symhacking && $tag{'TRUE'} != -1) {
				# This is for aliasing.  When we find an alias,
				# we have to invert
				&$make_variant($1,$2);
				print STDERR "DEBUG: $file: defined $1 = $2\n" if $debug;
			}
			if (/^\#/) {
				@current_platforms =
				    grep(!/^$/,
					 map { $tag{$_} == 1 ? $_ :
						   $tag{$_} == -1 ? "!".$_  : "" }
					 @known_platforms);
				push @current_platforms
				    , grep(!/^$/,
					   map { $tag{"OPENSSL_SYS_".$_} == 1 ? $_ :
						     $tag{"OPENSSL_SYS_".$_} == -1 ? "!".$_  : "" }
					   @known_ossl_platforms);
				@current_algorithms = ();
				@current_algorithms =
				    grep(!/^$/,
					 map { $tag{"OPENSSL_NO_".$_} == -1 ? $_ : "" }
					 @known_algorithms);
				push @current_algorithms
				    , grep(!/^$/,
					 map { $tag{"OPENSSL_USE_".$_} == 1 ? $_ : "" }
					 @known_algorithms);
				$def .=
				    "#INFO:"
					.join(',',@current_platforms).":"
					    .join(',',@current_algorithms).";";
				next;
			}
			if ($tag{'TRUE'} != -1) {
				if (/^\s*DEFINE_STACK_OF\s*\(\s*(\w*)\s*\)/
						|| /^\s*DEFINE_STACK_OF_CONST\s*\(\s*(\w*)\s*\)/) {
					next;
				} elsif (/^\s*DECLARE_ASN1_ENCODE_FUNCTIONS\s*\(\s*(\w*)\s*,\s*(\w*)\s*,\s*(\w*)\s*\)/) {
					$def .= "int d2i_$3(void);";
					$def .= "int i2d_$3(void);";
					# Variant for platforms that do not
					# have to access globale variables
					# in shared libraries through functions
					$def .=
					    "#INFO:"
						.join(',',"!EXPORT_VAR_AS_FUNCTION",@current_platforms).":"
						    .join(',',@current_algorithms).";";
					$def .= "OPENSSL_EXTERN int $2_it;";
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',@current_algorithms).";";
					# Variant for platforms that have to
					# access globale variables in shared
					# libraries through functions
					&$make_variant("$2_it","$2_it",
						      "EXPORT_VAR_AS_FUNCTION",
						      "FUNCTION");
					next;
				} elsif (/^\s*DECLARE_ASN1_FUNCTIONS_fname\s*\(\s*(\w*)\s*,\s*(\w*)\s*,\s*(\w*)\s*\)/) {
					$def .= "int d2i_$3(void);";
					$def .= "int i2d_$3(void);";
					$def .= "int $3_free(void);";
					$def .= "int $3_new(void);";
					# Variant for platforms that do not
					# have to access globale variables
					# in shared libraries through functions
					$def .=
					    "#INFO:"
						.join(',',"!EXPORT_VAR_AS_FUNCTION",@current_platforms).":"
						    .join(',',@current_algorithms).";";
					$def .= "OPENSSL_EXTERN int $2_it;";
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',@current_algorithms).";";
					# Variant for platforms that have to
					# access globale variables in shared
					# libraries through functions
					&$make_variant("$2_it","$2_it",
						      "EXPORT_VAR_AS_FUNCTION",
						      "FUNCTION");
					next;
				} elsif (/^\s*DECLARE_ASN1_FUNCTIONS\s*\(\s*(\w*)\s*\)/ ||
					 /^\s*DECLARE_ASN1_FUNCTIONS_const\s*\(\s*(\w*)\s*\)/) {
					$def .= "int d2i_$1(void);";
					$def .= "int i2d_$1(void);";
					$def .= "int $1_free(void);";
					$def .= "int $1_new(void);";
					# Variant for platforms that do not
					# have to access globale variables
					# in shared libraries through functions
					$def .=
					    "#INFO:"
						.join(',',"!EXPORT_VAR_AS_FUNCTION",@current_platforms).":"
						    .join(',',@current_algorithms).";";
					$def .= "OPENSSL_EXTERN int $1_it;";
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',@current_algorithms).";";
					# Variant for platforms that have to
					# access globale variables in shared
					# libraries through functions
					&$make_variant("$1_it","$1_it",
						      "EXPORT_VAR_AS_FUNCTION",
						      "FUNCTION");
					next;
				} elsif (/^\s*DECLARE_ASN1_ENCODE_FUNCTIONS_const\s*\(\s*(\w*)\s*,\s*(\w*)\s*\)/) {
					$def .= "int d2i_$2(void);";
					$def .= "int i2d_$2(void);";
					# Variant for platforms that do not
					# have to access globale variables
					# in shared libraries through functions
					$def .=
					    "#INFO:"
						.join(',',"!EXPORT_VAR_AS_FUNCTION",@current_platforms).":"
						    .join(',',@current_algorithms).";";
					$def .= "OPENSSL_EXTERN int $2_it;";
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',@current_algorithms).";";
					# Variant for platforms that have to
					# access globale variables in shared
					# libraries through functions
					&$make_variant("$2_it","$2_it",
						      "EXPORT_VAR_AS_FUNCTION",
						      "FUNCTION");
					next;
				} elsif (/^\s*DECLARE_ASN1_ALLOC_FUNCTIONS\s*\(\s*(\w*)\s*\)/) {
					$def .= "int $1_free(void);";
					$def .= "int $1_new(void);";
					next;
				} elsif (/^\s*DECLARE_ASN1_FUNCTIONS_name\s*\(\s*(\w*)\s*,\s*(\w*)\s*\)/) {
					$def .= "int d2i_$2(void);";
					$def .= "int i2d_$2(void);";
					$def .= "int $2_free(void);";
					$def .= "int $2_new(void);";
					# Variant for platforms that do not
					# have to access globale variables
					# in shared libraries through functions
					$def .=
					    "#INFO:"
						.join(',',"!EXPORT_VAR_AS_FUNCTION",@current_platforms).":"
						    .join(',',@current_algorithms).";";
					$def .= "OPENSSL_EXTERN int $2_it;";
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',@current_algorithms).";";
					# Variant for platforms that have to
					# access globale variables in shared
					# libraries through functions
					&$make_variant("$2_it","$2_it",
						      "EXPORT_VAR_AS_FUNCTION",
						      "FUNCTION");
					next;
				} elsif (/^\s*DECLARE_ASN1_ITEM\s*\(\s*(\w*)\s*\)/) {
					# Variant for platforms that do not
					# have to access globale variables
					# in shared libraries through functions
					$def .=
					    "#INFO:"
						.join(',',"!EXPORT_VAR_AS_FUNCTION",@current_platforms).":"
						    .join(',',@current_algorithms).";";
					$def .= "OPENSSL_EXTERN int $1_it;";
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',@current_algorithms).";";
					# Variant for platforms that have to
					# access globale variables in shared
					# libraries through functions
					&$make_variant("$1_it","$1_it",
						      "EXPORT_VAR_AS_FUNCTION",
						      "FUNCTION");
					next;
				} elsif (/^\s*DECLARE_ASN1_NDEF_FUNCTION\s*\(\s*(\w*)\s*\)/) {
					$def .= "int i2d_$1_NDEF(void);";
				} elsif (/^\s*DECLARE_ASN1_SET_OF\s*\(\s*(\w*)\s*\)/) {
					next;
				} elsif (/^\s*DECLARE_ASN1_PRINT_FUNCTION\s*\(\s*(\w*)\s*\)/) {
					$def .= "int $1_print_ctx(void);";
					next;
				} elsif (/^\s*DECLARE_ASN1_PRINT_FUNCTION_name\s*\(\s*(\w*)\s*,\s*(\w*)\s*\)/) {
					$def .= "int $2_print_ctx(void);";
					next;
				} elsif (/^\s*DECLARE_PKCS12_STACK_OF\s*\(\s*(\w*)\s*\)/) {
					next;
				} elsif (/^DECLARE_PEM_rw\s*\(\s*(\w*)\s*,/ ||
					 /^DECLARE_PEM_rw_cb\s*\(\s*(\w*)\s*,/ ||
					 /^DECLARE_PEM_rw_const\s*\(\s*(\w*)\s*,/ ) {
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',"STDIO",@current_algorithms).";";
					$def .= "int PEM_read_$1(void);";
					$def .= "int PEM_write_$1(void);";
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',@current_algorithms).";";
					# Things that are everywhere
					$def .= "int PEM_read_bio_$1(void);";
					$def .= "int PEM_write_bio_$1(void);";
					next;
				} elsif (/^DECLARE_PEM_write\s*\(\s*(\w*)\s*,/ ||
					/^DECLARE_PEM_write_const\s*\(\s*(\w*)\s*,/ ||
					 /^DECLARE_PEM_write_cb\s*\(\s*(\w*)\s*,/ ) {
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',"STDIO",@current_algorithms).";";
					$def .= "int PEM_write_$1(void);";
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',@current_algorithms).";";
					# Things that are everywhere
					$def .= "int PEM_write_bio_$1(void);";
					next;
				} elsif (/^DECLARE_PEM_read\s*\(\s*(\w*)\s*,/ ||
					 /^DECLARE_PEM_read_cb\s*\(\s*(\w*)\s*,/ ) {
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',"STDIO",@current_algorithms).";";
					$def .= "int PEM_read_$1(void);";
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',"STDIO",@current_algorithms).";";
					# Things that are everywhere
					$def .= "int PEM_read_bio_$1(void);";
					next;
				} elsif (/^OPENSSL_DECLARE_GLOBAL\s*\(\s*(\w*)\s*,\s*(\w*)\s*\)/) {
					# Variant for platforms that do not
					# have to access globale variables
					# in shared libraries through functions
					$def .=
					    "#INFO:"
						.join(',',"!EXPORT_VAR_AS_FUNCTION",@current_platforms).":"
						    .join(',',@current_algorithms).";";
					$def .= "OPENSSL_EXTERN int _shadow_$2;";
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',@current_algorithms).";";
					# Variant for platforms that have to
					# access globale variables in shared
					# libraries through functions
					&$make_variant("_shadow_$2","_shadow_$2",
						      "EXPORT_VAR_AS_FUNCTION",
						      "FUNCTION");
				} elsif (/^\s*DEPRECATEDIN/) {
					$parens = count_parens($_);
					if ($parens == 0) {
						$def .= do_deprecated($_,
							\@current_platforms,
							\@current_algorithms);
					} else {
						$stored_multiline = $_;
						print STDERR "DEBUG: Found multiline DEPRECATEDIN starting with: $stored_multiline\n" if $debug;
						next;
					}
				} elsif ($tag{'CONST_STRICT'} != 1) {
					if (/\{|\/\*|\([^\)]*$/) {
						$line = $_;
					} else {
						$def .= $_;
					}
				}
			}
		}
		close(IN);
		die "$file: Unmatched tags\n" if $#tag >= 0;

		my $algs;
		my $plays;

		print STDERR "DEBUG: postprocessing ----------\n" if $debug;
		foreach (split /;/, $def) {
			my $s; my $k = "FUNCTION"; my $p; my $a;
			s/^[\n\s]*//g;
			s/[\n\s]*$//g;
			next if(/\#undef/);
			next if(/typedef\W/);
			next if(/\#define/);

			print STDERR "TRACE: processing $_\n" if $trace && !/^\#INFO:/;
			# Reduce argument lists to empty ()
			# fold round brackets recursively: (t(*v)(t),t) -> (t{}{},t) -> {}
			my $nsubst = 1; # prevent infinite loop, e.g., on  int fn()
			while($nsubst && /\(.*\)/s) {
				$nsubst = s/\([^\(\)]+\)/\{\}/gs;
				$nsubst+= s/\(\s*\*\s*(\w+)\s*\{\}\s*\)/$1/gs;	#(*f{}) -> f
			}
			# pretend as we didn't use curly braces: {} -> ()
			s/\{\}/\(\)/gs;

			s/STACK_OF\(\)/void/gs;
			s/LHASH_OF\(\)/void/gs;

			print STDERR "DEBUG: \$_ = \"$_\"\n" if $debug;
			if (/^\#INFO:([^:]*):(.*)$/) {
				$plats = $1;
				$algs = $2;
				print STDERR "DEBUG: found info on platforms ($plats) and algorithms ($algs)\n" if $debug;
				next;
			} elsif (/^\s*OPENSSL_EXTERN\s.*?(\w+(\{[0-9]+\})?)(\[[0-9]*\])*\s*$/) {
				$s = $1;
				$k = "VARIABLE";
				print STDERR "DEBUG: found external variable $s\n" if $debug;
			} elsif (/TYPEDEF_\w+_OF/s) {
				next;
			} elsif (/(\w+)\s*\(\).*/s) {	# first token prior [first] () is
				$s = $1;		# a function name!
				print STDERR "DEBUG: found function $s\n" if $debug;
			} elsif (/\(/ and not (/=/)) {
				print STDERR "File $file: cannot parse: $_;\n";
				next;
			} else {
				next;
			}

			$syms{$s} = 1;
			$kind{$s} = $k;

			$p = $plats;
			$a = $algs;

			$platform{$s} =
			    &reduce_platforms((defined($platform{$s})?$platform{$s}.',':"").$p);
			$algorithm{$s} .= ','.$a;

			if (defined($variant{$s})) {
				foreach $v (split /;/,$variant{$s}) {
					(my $r, my $p, my $k) = split(/:/,$v);
					my $ip = join ',',map({ /^!(.*)$/ ? $1 : "!".$_ } split /,/, $p);
					$syms{$r} = 1;
					if (!defined($k)) { $k = $kind{$s}; }
					$kind{$r} = $k."(".$s.")";
					$algorithm{$r} = $algorithm{$s};
					$platform{$r} = &reduce_platforms($platform{$s}.",".$p.",".$p);
					$platform{$s} = &reduce_platforms($platform{$s}.','.$ip.','.$ip);
					print STDERR "DEBUG: \$variant{\"$s\"} = ",$v,"; \$r = $r; \$p = ",$platform{$r},"; \$a = ",$algorithm{$r},"; \$kind = ",$kind{$r},"\n" if $debug;
				}
			}
			print STDERR "DEBUG: \$s = $s; \$p = ",$platform{$s},"; \$a = ",$algorithm{$s},"; \$kind = ",$kind{$s},"\n" if $debug;
		}
	}

	# Info we know about

	push @ret, map { $_."\\".&info_string($_,"EXIST",
					      $platform{$_},
					      $kind{$_},
					      $algorithm{$_}) } keys %syms;

	if (keys %unknown_algorithms) {
		print STDERR "WARNING: mkdef.pl doesn't know the following algorithms:\n";
		print STDERR "\t",join("\n\t",keys %unknown_algorithms),"\n";
	}
	return(@ret);
}

# Param: string of comma-separated platform-specs.
sub reduce_platforms
{
	my ($platforms) = @_;
	my $pl = defined($platforms) ? $platforms : "";
	my %p = map { $_ => 0 } split /,/, $pl;
	my $ret;

	print STDERR "DEBUG: Entered reduce_platforms with \"$platforms\"\n"
	    if $debug;
	# We do this, because if there's code like the following, it really
	# means the function exists in all cases and should therefore be
	# everywhere.  By increasing and decreasing, we may attain 0:
	#
	# ifndef WIN16
	#    int foo();
	# else
	#    int _fat foo();
	# endif
	foreach $platform (split /,/, $pl) {
		if ($platform =~ /^!(.*)$/) {
			$p{$1}--;
		} else {
			$p{$platform}++;
		}
	}
	foreach $platform (keys %p) {
		if ($p{$platform} == 0) { delete $p{$platform}; }
	}

	delete $p{""};

	$ret = join(',',sort(map { $p{$_} < 0 ? "!".$_ : $_ } keys %p));
	print STDERR "DEBUG: Exiting reduce_platforms with \"$ret\"\n"
	    if $debug;
	return $ret;
}

sub info_string
{
	(my $symbol, my $exist, my $platforms, my $kind, my $algorithms) = @_;

	my %a = defined($algorithms) ?
	    map { $_ => 1 } split /,/, $algorithms : ();
	my $k = defined($kind) ? $kind : "FUNCTION";
	my $ret;
	my $p = &reduce_platforms($platforms);

	delete $a{""};

	$ret = $exist;
	$ret .= ":".$p;
	$ret .= ":".$k;
	$ret .= ":".join(',',sort keys %a);
	return $ret;
}

sub maybe_add_info
{
	(my $name, *nums, my @symbols) = @_;
	my $sym;
	my $new_info = 0;
	my %syms=();

	foreach $sym (@symbols) {
		(my $s, my $i) = split /\\/, $sym;
		if (defined($nums{$s})) {
			$i =~ s/^(.*?:.*?:\w+)(\(\w+\))?/$1/;
			(my $n, my $vers, my $dummy) = split /\\/, $nums{$s};
			if (!defined($dummy) || $i ne $dummy) {
				$nums{$s} = $n."\\".$vers."\\".$i;
				$new_info++;
				print STDERR "DEBUG: maybe_add_info for $s: \"$dummy\" => \"$i\"\n" if $debug;
			}
		}
		$syms{$s} = 1;
	}

	my @s=sort { &parse_number($nums{$a},"n") <=> &parse_number($nums{$b},"n") } keys %nums;
	foreach $sym (@s) {
		(my $n, my $vers, my $i) = split /\\/, $nums{$sym};
		if (!defined($syms{$sym}) && $i !~ /^NOEXIST:/) {
			$new_info++;
			print STDERR "DEBUG: maybe_add_info for $sym: -> undefined\n" if $debug;
		}
	}
	if ($new_info) {
		print STDERR "$name: $new_info old symbols have updated info\n";
		if (!$do_rewrite) {
			print STDERR "You should do a rewrite to fix this.\n";
		}
	} else {
	}
}

# Param: string of comma-separated keywords, each possibly prefixed with a "!"
sub is_valid
{
	my ($keywords_txt,$platforms) = @_;
	my (@keywords) = split /,/,$keywords_txt;
	my ($falsesum, $truesum) = (0, 1);

	# Param: one keyword
	sub recognise
	{
		my ($keyword,$platforms) = @_;

		if ($platforms) {
			# platforms
			if ($keyword eq "UNIX" && $UNIX) { return 1; }
			if ($keyword eq "VMS" && $VMS) { return 1; }
			if ($keyword eq "WIN32" && $W32) { return 1; }
			if ($keyword eq "_WIN32" && $W32) { return 1; }
			if ($keyword eq "WINNT" && $NT) { return 1; }
			# Special platforms:
			# EXPORT_VAR_AS_FUNCTION means that global variables
			# will be represented as functions.
			if ($keyword eq "EXPORT_VAR_AS_FUNCTION" && $W32) {
				return 1;
			}
			if ($keyword eq "ZLIB" && $zlib) { return 1; }
			return 0;
		} else {
			# algorithms
			if ($disabled_algorithms{$keyword} == 1) { return 0;}

			# Nothing recognise as true
			return 1;
		}
	}

	foreach $k (@keywords) {
		if ($k =~ /^!(.*)$/) {
			$falsesum += &recognise($1,$platforms);
		} else {
			$truesum *= &recognise($k,$platforms);
		}
	}
	print STDERR "DEBUG: [",$#keywords,",",$#keywords < 0,"] is_valid($keywords_txt) => (\!$falsesum) && $truesum = ",(!$falsesum) && $truesum,"\n" if $debug;
	return (!$falsesum) && $truesum;
}

sub print_test_file
{
	(*OUT,my $name,*nums,my $testall,my @symbols)=@_;
	my $n = 1; my @e; my @r;
	my $sym; my $prev = ""; my $prefSSLeay;

	(@e)=grep(/^SSLeay(\{[0-9]+\})?\\.*?:.*?:.*/,@symbols);
	(@r)=grep(/^\w+(\{[0-9]+\})?\\.*?:.*?:.*/ && !/^SSLeay(\{[0-9]+\})?\\.*?:.*?:.*/,@symbols);
	@symbols=((sort @e),(sort @r));

	foreach $sym (@symbols) {
		(my $s, my $i) = $sym =~ /^(.*?)\\(.*)$/;
		my $v = 0;
		$v = 1 if $i=~ /^.*?:.*?:VARIABLE/;
		my $p = ($i =~ /^[^:]*:([^:]*):/,$1);
		my $a = ($i =~ /^[^:]*:[^:]*:[^:]*:([^:]*)/,$1);
		if (!defined($nums{$s})) {
			print STDERR "Warning: $s does not have a number assigned\n"
			    if(!$do_update);
		} elsif (is_valid($p,1) && is_valid($a,0)) {
			my $s2 = ($s =~ /^(.*?)(\{[0-9]+\})?$/, $1);
			if ($prev eq $s2) {
				print OUT "\t/* The following has already appeared previously */\n";
				print STDERR "Warning: Symbol '",$s2,"' redefined. old=",($nums{$prev} =~ /^(.*?)\\/,$1),", new=",($nums{$s2} =~ /^(.*?)\\/,$1),"\n";
			}
			$prev = $s2;	# To warn about duplicates...

			(my $nn, my $vers, my $ni) = split /\\/, $nums{$s2};
			if ($v) {
				print OUT "\textern int $s2; /* type unknown */ /* $nn $ni */\n";
			} else {
				print OUT "\textern int $s2(); /* type unknown */ /* $nn $ni */\n";
			}
		}
	}
}

sub get_version
{
   return $config{version};
}

sub print_def_file
{
	(*OUT,my $name,*nums,my @symbols)=@_;
	my $n = 1; my @e; my @r; my @v; my $prev="";
	my $liboptions="";
	my $libname = $name;
	my $http_vendor = 'www.openssl.org/';
	my $version = get_version();
	my $what = "OpenSSL: implementation of Secure Socket Layer";
	my $description = "$what $version, $name - http://$http_vendor";
	my $prevsymversion = "", $prevprevsymversion = "";
        # For VMS
        my $prevnum = 0;
        my $symvtextcount = 0;

	if ($W32)
		{ $libname.="32"; }

        if ($W32)
                {
                print OUT <<"EOF";
;
; Definition file for the DLL version of the $name library from OpenSSL
;

LIBRARY         $libname	$liboptions

EOF

		print "EXPORTS\n";
                }
        elsif ($VMS)
                {
                print OUT <<"EOF";
CASE_SENSITIVE=YES
SYMBOL_VECTOR=(-
EOF
                $symvtextcount = 16; # length of "SYMBOL_VECTOR=(-"
                }

	(@r)=grep(/^\w+(\{[0-9]+\})?\\.*?:.*?:FUNCTION/,@symbols);
	(@v)=grep(/^\w+(\{[0-9]+\})?\\.*?:.*?:VARIABLE/,@symbols);
        if ($VMS) {
            # VMS needs to have the symbols on slot number order
            @symbols=(map { $_->[1] }
                      sort { $a->[0] <=> $b->[0] }
                      map { (my $s, my $i) = $_ =~ /^(.*?)\\(.*)$/;
                            die "Error: $s doesn't have a number assigned\n"
                                if !defined($nums{$s});
                            (my $n, my @rest) = split /\\/, $nums{$s};
                            [ $n, $_ ] } (@e, @r, @v));
        } else {
            @symbols=((sort @e),(sort @r), (sort @v));
        }

	my ($baseversion, $currversion) = get_openssl_version();
	my $thisversion;
	do {
		if (!defined($thisversion)) {
			$thisversion = $baseversion;
		} else {
			$thisversion = get_next_version($thisversion);
		}
		foreach $sym (@symbols) {
			(my $s, my $i) = $sym =~ /^(.*?)\\(.*)$/;
			my $v = 0;
			$v = 1 if $i =~ /^.*?:.*?:VARIABLE/;
			if (!defined($nums{$s})) {
				die "Error: $s does not have a number assigned\n"
					if(!$do_update);
			} else {
				(my $n, my $symversion, my $dummy) = split /\\/, $nums{$s};
				my %pf = ();
				my $p = ($i =~ /^[^:]*:([^:]*):/,$1);
				my $a = ($i =~ /^[^:]*:[^:]*:[^:]*:([^:]*)/,$1);
				if (is_valid($p,1) && is_valid($a,0)) {
					my $s2 = ($s =~ /^(.*?)(\{[0-9]+\})?$/, $1);
					if ($prev eq $s2) {
						print STDERR "Warning: Symbol '",$s2,
							"' redefined. old=",($nums{$prev} =~ /^(.*?)\\/,$1),
							", new=",($nums{$s2} =~ /^(.*?)\\/,$1),"\n";
					}
					$prev = $s2;	# To warn about duplicates...
					if($linux) {
						next if $symversion ne $thisversion;
						if ($symversion ne $prevsymversion) {
							if ($prevsymversion ne "") {
								if ($prevprevsymversion ne "") {
									print OUT "} OPENSSL${SO_VARIANT}_"
												."$prevprevsymversion;\n\n";
								} else {
									print OUT "};\n\n";
								}
							}
							print OUT "OPENSSL${SO_VARIANT}_$symversion {\n    global:\n";
							$prevprevsymversion = $prevsymversion;
							$prevsymversion = $symversion;
						}
						print OUT "        $s2;\n";
                                        } elsif ($VMS) {
                                            while(++$prevnum < $n) {
                                                my $symline=" ,SPARE -\n  ,SPARE -\n";
                                                if ($symvtextcount + length($symline) - 2 > 1024) {
                                                    print OUT ")\nSYMBOL_VECTOR=(-\n";
                                                    $symvtextcount = 16; # length of "SYMBOL_VECTOR=(-"
                                                }
                                                if ($symvtextcount == 16) {
                                                    # Take away first comma
                                                    $symline =~ s/,//;
                                                }
                                                print OUT $symline;
                                                $symvtextcount += length($symline) - 2;
                                            }
                                            (my $s_uc = $s) =~ tr/a-z/A-Z/;
                                            my $symtype=
                                                $v ? "DATA" : "PROCEDURE";
                                            my $symline=
                                                ($s_uc ne $s
                                                 ? " ,$s_uc/$s=$symtype -\n  ,$s=$symtype -\n"
                                                 : " ,$s=$symtype -\n  ,SPARE -\n");
                                            if ($symvtextcount + length($symline) - 2 > 1024) {
                                                print OUT ")\nSYMBOL_VECTOR=(-\n";
                                                $symvtextcount = 16; # length of "SYMBOL_VECTOR=(-"
                                            }
                                            if ($symvtextcount == 16) {
                                                # Take away first comma
                                                $symline =~ s/,//;
                                            }
                                            print OUT $symline;
                                            $symvtextcount += length($symline) - 2;
					} elsif($v) {
						printf OUT "    %s%-39s DATA\n",
								($W32)?"":"_",$s2;
					} else {
						printf OUT "    %s%s\n",
								($W32)?"":"_",$s2;
					}
				}
			}
		}
	} while ($linux && $thisversion ne $currversion);
	if ($linux) {
		if ($prevprevsymversion ne "") {
			print OUT "    local: *;\n} OPENSSL${SO_VARIANT}_$prevprevsymversion;\n\n";
		} else {
			print OUT "    local: *;\n};\n\n";
		}
	} elsif ($VMS) {
            print OUT ")\n";
            (my $libvmaj, my $libvmin, my $libvedit) =
                $currversion =~ /^(\d+)_(\d+)_(\d+)[a-z]{0,2}$/;
            # The reason to multiply the edit number with 100 is to make space
            # for the possibility that we want to encode the patch letters
            print OUT "GSMATCH=LEQUAL,",($libvmaj * 100 + $libvmin),",",($libvedit * 100),"\n";
        }
	printf OUT "\n";
}

sub load_numbers
{
	my($name)=@_;
	my(@a,%ret);
	my $prevversion;

	$max_num = 0;
	$num_noinfo = 0;
	$prev = "";
	$prev_cnt = 0;

	my ($baseversion, $currversion) = get_openssl_version();

	open(IN,"<$name") || die "unable to open $name:$!\n";
	while (<IN>) {
		s|\R$||;        # Better chomp
		s/#.*$//;
		next if /^\s*$/;
		@a=split;
		if (defined $ret{$a[0]}) {
			# This is actually perfectly OK
			#print STDERR "Warning: Symbol '",$a[0],"' redefined. old=",$ret{$a[0]},", new=",$a[1],"\n";
		}
		if ($max_num > $a[1]) {
			print STDERR "Warning: Number decreased from ",$max_num," to ",$a[1],"\n";
		}
		elsif ($max_num == $a[1]) {
			# This is actually perfectly OK
			#print STDERR "Warning: Symbol ",$a[0]," has same number as previous ",$prev,": ",$a[1],"\n";
			if ($a[0] eq $prev) {
				$prev_cnt++;
				$a[0] .= "{$prev_cnt}";
			}
		}
		else {
			$prev_cnt = 0;
		}
		if ($#a < 2) {
			# Existence will be proven later, in do_defs
			$ret{$a[0]}=$a[1];
			$num_noinfo++;
		} else {
			#Sanity check the version number
			if (defined $prevversion) {
				check_version_lte($prevversion, $a[2]);
			}
			check_version_lte($a[2], $currversion);
			$prevversion = $a[2];
			$ret{$a[0]}=$a[1]."\\".$a[2]."\\".$a[3]; # \\ is a special marker
		}
		$max_num = $a[1] if $a[1] > $max_num;
		$prev=$a[0];
	}
	if ($num_noinfo) {
		print STDERR "Warning: $num_noinfo symbols were without info." if $verbose || !$do_rewrite;
		if ($do_rewrite) {
			printf STDERR "  The rewrite will fix this.\n" if $verbose;
		} else {
			printf STDERR "  You should do a rewrite to fix this.\n";
		}
	}
	close(IN);
	return(%ret);
}

sub parse_number
{
	(my $str, my $what) = @_;
	(my $n, my $v, my $i) = split(/\\/,$str);
	if ($what eq "n") {
		return $n;
	} else {
		return $i;
	}
}

sub rewrite_numbers
{
	(*OUT,$name,*nums,@symbols)=@_;
	my $thing;

	my @r = grep(/^\w+(\{[0-9]+\})?\\.*?:.*?:\w+\(\w+\)/,@symbols);
	my $r; my %r; my %rsyms;
	foreach $r (@r) {
		(my $s, my $i) = split /\\/, $r;
		my $a = $1 if $i =~ /^.*?:.*?:\w+\((\w+)\)/;
		$i =~ s/^(.*?:.*?:\w+)\(\w+\)/$1/;
		$r{$a} = $s."\\".$i;
		$rsyms{$s} = 1;
	}

	my %syms = ();
	foreach $_ (@symbols) {
		(my $n, my $i) = split /\\/;
		$syms{$n} = 1;
	}

	my @s=sort {
	    &parse_number($nums{$a},"n") <=> &parse_number($nums{$b},"n")
	    || $a cmp $b
	} keys %nums;
	foreach $sym (@s) {
		(my $n, my $vers, my $i) = split /\\/, $nums{$sym};
		next if defined($i) && $i =~ /^.*?:.*?:\w+\(\w+\)/;
		next if defined($rsyms{$sym});
		print STDERR "DEBUG: rewrite_numbers for sym = ",$sym,": i = ",$i,", n = ",$n,", rsym{sym} = ",$rsyms{$sym},"syms{sym} = ",$syms{$sym},"\n" if $debug;
		$i="NOEXIST::FUNCTION:"
			if !defined($i) || $i eq "" || !defined($syms{$sym});
		my $s2 = $sym;
		$s2 =~ s/\{[0-9]+\}$//;
		printf OUT "%s%-39s %d\t%s\t%s\n","",$s2,$n,$vers,$i;
		if (exists $r{$sym}) {
			(my $s, $i) = split /\\/,$r{$sym};
			my $s2 = $s;
			$s2 =~ s/\{[0-9]+\}$//;
			printf OUT "%s%-39s %d\t%s\t%s\n","",$s2,$n,$vers,$i;
		}
	}
}

sub update_numbers
{
	(*OUT,$name,*nums,my $start_num, my @symbols)=@_;
	my $new_syms = 0;
	my $basevers;
	my $vers;

	($basevers, $vers) = get_openssl_version();

	my @r = grep(/^\w+(\{[0-9]+\})?\\.*?:.*?:\w+\(\w+\)/,@symbols);
	my $r; my %r; my %rsyms;
	foreach $r (@r) {
		(my $s, my $i) = split /\\/, $r;
		my $a = $1 if $i =~ /^.*?:.*?:\w+\((\w+)\)/;
		$i =~ s/^(.*?:.*?:\w+)\(\w+\)/$1/;
		$r{$a} = $s."\\".$i;
		$rsyms{$s} = 1;
	}

	foreach $sym (@symbols) {
		(my $s, my $i) = $sym =~ /^(.*?)\\(.*)$/;
		next if $i =~ /^.*?:.*?:\w+\(\w+\)/;
		next if defined($rsyms{$sym});
		die "ERROR: Symbol $sym had no info attached to it."
		    if $i eq "";
		if (!exists $nums{$s}) {
			$new_syms++;
			my $s2 = $s;
			$s2 =~ s/\{[0-9]+\}$//;
			printf OUT "%s%-39s %d\t%s\t%s\n","",$s2, ++$start_num,$vers,$i;
			if (exists $r{$s}) {
				($s, $i) = split /\\/,$r{$s};
				$s =~ s/\{[0-9]+\}$//;
				printf OUT "%s%-39s %d\t%s\t%s\n","",$s, $start_num,$vers,$i;
			}
		}
	}
	if($new_syms) {
		print STDERR "$name: Added $new_syms new symbols\n";
	} else {
		print STDERR "$name: No new symbols added\n";
	}
}

sub check_existing
{
	(*nums, my @symbols)=@_;
	my %existing; my @remaining;
	@remaining=();
	foreach $sym (@symbols) {
		(my $s, my $i) = $sym =~ /^(.*?)\\(.*)$/;
		$existing{$s}=1;
	}
	foreach $sym (keys %nums) {
		if (!exists $existing{$sym}) {
			push @remaining, $sym;
		}
	}
	if(@remaining) {
		print STDERR "The following symbols do not seem to exist:\n";
		foreach $sym (@remaining) {
			print STDERR "\t",$sym,"\n";
		}
	}
}

sub count_parens
{
	my $line = shift(@_);

	my $open = $line =~ tr/\(//;
	my $close = $line =~ tr/\)//;

	return $open - $close;
}

#Parse opensslv.h to get the current version number. Also work out the base
#version, i.e. the lowest version number that is binary compatible with this
#version
sub get_openssl_version()
{
	my $fn = catfile($config{sourcedir},"include","openssl","opensslv.h");
	open (IN, "$fn") || die "Can't open opensslv.h";

	while(<IN>) {
		if (/OPENSSL_VERSION_TEXT\s+"OpenSSL (\d\.\d\.)(\d[a-z]*)(-| )/) {
			my $suffix = $2;
			(my $baseversion = $1) =~ s/\./_/g;
			close IN;
			return ($baseversion."0", $baseversion.$suffix);
		}
	}
	die "Can't find OpenSSL version number\n";
}

#Given an OpenSSL version number, calculate the next version number. If the
#version number gets to a.b.czz then we go to a.b.(c+1)
sub get_next_version()
{
	my $thisversion = shift;

	my ($base, $letter) = $thisversion =~ /^(\d_\d_\d)([a-z]{0,2})$/;

	if ($letter eq "zz") {
		my $lastnum = substr($base, -1);
		return substr($base, 0, length($base)-1).(++$lastnum);
	}
	return $base.get_next_letter($letter);
}

#Given the letters off the end of an OpenSSL version string, calculate what
#the letters for the next release would be.
sub get_next_letter()
{
	my $thisletter = shift;
	my $baseletter = "";
	my $endletter;

	if ($thisletter eq "") {
		return "a";
	}
	if ((length $thisletter) > 1) {
		($baseletter, $endletter) = $thisletter =~ /([a-z]+)([a-z])/;
	} else {
		$endletter = $thisletter;
	}

	if ($endletter eq "z") {
		return $thisletter."a";
	} else {
		return $baseletter.(++$endletter);
	}
}

#Check if a version is less than or equal to the current version. Its a fatal
#error if not. They must also only differ in letters, or the last number (i.e.
#the first two numbers must be the same)
sub check_version_lte()
{
	my ($testversion, $currversion) = @_;
	my $lentv;
	my $lencv;
	my $cvbase;

	my ($cvnums) = $currversion =~ /^(\d_\d_\d)[a-z]*$/;
	my ($tvnums) = $testversion =~ /^(\d_\d_\d)[a-z]*$/;

	#Die if we can't parse the version numbers or they don't look sane
	die "Invalid version number: $testversion and $currversion\n"
		if (!defined($cvnums) || !defined($tvnums)
			|| length($cvnums) != 5
			|| length($tvnums) != 5);

	#If the base versions (without letters) don't match check they only differ
	#in the last number
	if ($cvnums ne $tvnums) {
		die "Invalid version number: $testversion "
			."for current version $currversion\n"
			if (substr($cvnums, -1) < substr($tvnums, -1)
				|| substr($cvnums, 0, 4) ne substr($tvnums, 0, 4));
		return;
	}
	#If we get here then the base version (i.e. the numbers) are the same - they
	#only differ in the letters

	$lentv = length $testversion;
	$lencv = length $currversion;

	#If the testversion has more letters than the current version then it must
	#be later (or malformed)
	if ($lentv > $lencv) {
		die "Invalid version number: $testversion "
			."is greater than $currversion\n";
	}

	#Get the last letter from the current version
	my ($cvletter) = $currversion =~ /([a-z])$/;
	if (defined $cvletter) {
		($cvbase) = $currversion =~ /(\d_\d_\d[a-z]*)$cvletter$/;
	} else {
		$cvbase = $currversion;
	}
	die "Unable to parse version number $currversion" if (!defined $cvbase);
	my $tvbase;
	my ($tvletter) = $testversion =~ /([a-z])$/;
	if (defined $tvletter) {
		($tvbase) = $testversion =~ /(\d_\d_\d[a-z]*)$tvletter$/;
	} else {
		$tvbase = $testversion;
	}
	die "Unable to parse version number $testversion" if (!defined $tvbase);

	if ($lencv > $lentv) {
		#If current version has more letters than testversion then testversion
		#minus the final letter must be a substring of the current version
		die "Invalid version number $testversion "
			."is greater than $currversion or is invalid\n"
			if (index($cvbase, $tvbase) != 0);
	} else {
		#If both versions have the same number of letters then they must be
		#equal up to the last letter, and the last letter in testversion must
		#be less than or equal to the last letter in current version.
		die "Invalid version number $testversion "
			."is greater than $currversion\n"
			if (($cvbase ne $tvbase) && ($tvletter gt $cvletter));
	}
}

sub do_deprecated()
{
	my ($decl, $plats, $algs) = @_;
	$decl =~ /^\s*(DEPRECATEDIN_\d+_\d+_\d+)\s*\((.*)\)\s*$/
            or die "Bad DEPRECTEDIN: $decl\n";
	my $info1 .= "#INFO:";
	$info1 .= join(',', @{$plats}) . ":";
	my $info2 = $info1;
	$info1 .= join(',',@{$algs}, $1) . ";";
	$info2 .= join(',',@{$algs}) . ";";
	return $info1 . $2 . ";" . $info2;
}
