#!/usr/local/bin/perl -w
#
# generate a .def file
#
# It does this by parsing the header files and looking for the
# prototyped functions: it then prunes the output.
#
# Intermediary files are created, call libeay.num and ssleay.num,...
# Previously, they had the following format:
#
#	routine-name	nnnn
#
# But that isn't enough for a number of reasons, the first on being that
# this format is (needlessly) very Win32-centric, and even then...
# One of the biggest problems is that there's no information about what
# routines should actually be used, which varies with what crypto algorithms
# are disabled.  Also, some operating systems (for example VMS with VAX C)
# need to keep track of the global variables as well as the functions.
#
# So, a remake of this script is done so as to include information on the
# kind of symbol it is (function or variable) and what algorithms they're
# part of.  This will allow easy translating to .def files or the corresponding
# file in other operating systems (a .opt file for VMS, possibly with a .mar
# file).
#
# The format now becomes:
#
#	routine-name	nnnn	info
#
# and the "info" part is actually a colon-separated string of fields with
# the following meaning:
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

my $debug=0;

my $crypto_num= "util/libeay.num";
my $ssl_num=    "util/ssleay.num";
my $libname;

my $do_update = 0;
my $do_rewrite = 1;
my $do_crypto = 0;
my $do_ssl = 0;
my $do_ctest = 0;
my $do_ctestall = 0;
my $do_checkexist = 0;

my $VMSVAX=0;
my $VMSNonVAX=0;
my $VMS=0;
my $W32=0;
my $W16=0;
my $NT=0;
my $OS2=0;
# Set this to make typesafe STACK definitions appear in DEF
my $safe_stack_def = 0;

my @known_platforms = ( "__FreeBSD__", "PERL5", "NeXT",
			"EXPORT_VAR_AS_FUNCTION", "ZLIB", "OPENSSL_FIPS" );
my @known_ossl_platforms = ( "VMS", "WIN16", "WIN32", "WINNT", "OS2" );
my @known_algorithms = ( "RC2", "RC4", "RC5", "IDEA", "DES", "BF",
			 "CAST", "MD2", "MD4", "MD5", "SHA", "SHA0", "SHA1",
			 "SHA256", "SHA512", "RIPEMD",
			 "MDC2", "WHIRLPOOL", "RSA", "DSA", "DH", "EC", "ECDH", "ECDSA", "EC2M",
			 "HMAC", "AES", "CAMELLIA", "SEED", "GOST",
			 # EC_NISTP_64_GCC_128
			 "EC_NISTP_64_GCC_128",
			 # Envelope "algorithms"
			 "EVP", "X509", "ASN1_TYPEDEFS",
			 # Helper "algorithms"
			 "BIO", "COMP", "BUFFER", "LHASH", "STACK", "ERR",
			 "LOCKING",
			 # External "algorithms"
			 "FP_API", "STDIO", "SOCK", "KRB5", "DGRAM",
			 # Engines
			 "STATIC_ENGINE", "ENGINE", "HW", "GMP",
			 # RFC3779
			 "RFC3779",
			 # TLS
			 "TLSEXT", "PSK", "SRP", "HEARTBEATS",
			 # CMS
			 "CMS",
			 # CryptoAPI Engine
			 "CAPIENG",
			 # SSL v2
			 "SSL2",
			 # SSL v2 method
			 "SSL2_METHOD",
			 # SSL v3 method
			 "SSL3_METHOD",
			 # JPAKE
			 "JPAKE",
			 # NEXTPROTONEG
			 "NEXTPROTONEG",
			 # Deprecated functions
			 "DEPRECATED",
			 # Hide SSL internals
			 "SSL_INTERN",
			 # SCTP
		 	 "SCTP",
			 # SRTP
			 "SRTP",
			 # Unit testing
		 	 "UNIT_TEST");

my $options="";
open(IN,"<Makefile") || die "unable to open Makefile!\n";
while(<IN>) {
    $options=$1 if (/^OPTIONS=(.*)$/);
}
close(IN);

# The following ciphers may be excluded (by Configure). This means functions
# defined with ifndef(NO_XXX) are not included in the .def file, and everything
# in directory xxx is ignored.
my $no_rc2; my $no_rc4; my $no_rc5; my $no_idea; my $no_des; my $no_bf;
my $no_cast; my $no_whirlpool; my $no_camellia; my $no_seed;
my $no_md2; my $no_md4; my $no_md5; my $no_sha; my $no_ripemd; my $no_mdc2;
my $no_rsa; my $no_dsa; my $no_dh; my $no_hmac=0; my $no_aes; my $no_krb5;
my $no_ec; my $no_ecdsa; my $no_ecdh; my $no_engine; my $no_hw;
my $no_fp_api; my $no_static_engine=1; my $no_gmp; my $no_deprecated;
my $no_rfc3779; my $no_psk; my $no_tlsext; my $no_cms; my $no_capieng;
my $no_jpake; my $no_srp; my $no_ssl2; my $no_ec2m; my $no_nistp_gcc; 
my $no_nextprotoneg; my $no_sctp; my $no_srtp;
my $no_unit_test; my $no_ssl3_method; my $no_ssl2_method;

my $fips;

my $zlib;


foreach (@ARGV, split(/ /, $options))
	{
	$debug=1 if $_ eq "debug";
	$W32=1 if $_ eq "32";
	$W16=1 if $_ eq "16";
	if($_ eq "NT") {
		$W32 = 1;
		$NT = 1;
	}
	if ($_ eq "VMS-VAX") {
		$VMS=1;
		$VMSVAX=1;
	}
	if ($_ eq "VMS-NonVAX") {
		$VMS=1;
		$VMSNonVAX=1;
	}
	$VMS=1 if $_ eq "VMS";
	$OS2=1 if $_ eq "OS2";
	$fips=1 if /^fips/;
	if ($_ eq "zlib" || $_ eq "enable-zlib" || $_ eq "zlib-dynamic"
			 || $_ eq "enable-zlib-dynamic") {
		$zlib = 1;
	}

	$do_ssl=1 if $_ eq "ssleay";
	if ($_ eq "ssl") {
		$do_ssl=1; 
		$libname=$_
	}
	$do_crypto=1 if $_ eq "libeay";
	if ($_ eq "crypto") {
		$do_crypto=1;
		$libname=$_;
	}
	$no_static_engine=1 if $_ eq "no-static-engine";
	$no_static_engine=0 if $_ eq "enable-static-engine";
	$do_update=1 if $_ eq "update";
	$do_rewrite=1 if $_ eq "rewrite";
	$do_ctest=1 if $_ eq "ctest";
	$do_ctestall=1 if $_ eq "ctestall";
	$do_checkexist=1 if $_ eq "exist";
	#$safe_stack_def=1 if $_ eq "-DDEBUG_SAFESTACK";

	if    (/^no-rc2$/)      { $no_rc2=1; }
	elsif (/^no-rc4$/)      { $no_rc4=1; }
	elsif (/^no-rc5$/)      { $no_rc5=1; }
	elsif (/^no-idea$/)     { $no_idea=1; }
	elsif (/^no-des$/)      { $no_des=1; $no_mdc2=1; }
	elsif (/^no-bf$/)       { $no_bf=1; }
	elsif (/^no-cast$/)     { $no_cast=1; }
	elsif (/^no-whirlpool$/)     { $no_whirlpool=1; }
	elsif (/^no-md2$/)      { $no_md2=1; }
	elsif (/^no-md4$/)      { $no_md4=1; }
	elsif (/^no-md5$/)      { $no_md5=1; }
	elsif (/^no-sha$/)      { $no_sha=1; }
	elsif (/^no-ripemd$/)   { $no_ripemd=1; }
	elsif (/^no-mdc2$/)     { $no_mdc2=1; }
	elsif (/^no-rsa$/)      { $no_rsa=1; }
	elsif (/^no-dsa$/)      { $no_dsa=1; }
	elsif (/^no-dh$/)       { $no_dh=1; }
	elsif (/^no-ec$/)       { $no_ec=1; }
	elsif (/^no-ecdsa$/)	{ $no_ecdsa=1; }
	elsif (/^no-ecdh$/) 	{ $no_ecdh=1; }
	elsif (/^no-hmac$/)	{ $no_hmac=1; }
	elsif (/^no-aes$/)	{ $no_aes=1; }
	elsif (/^no-camellia$/)	{ $no_camellia=1; }
	elsif (/^no-seed$/)     { $no_seed=1; }
	elsif (/^no-evp$/)	{ $no_evp=1; }
	elsif (/^no-lhash$/)	{ $no_lhash=1; }
	elsif (/^no-stack$/)	{ $no_stack=1; }
	elsif (/^no-err$/)	{ $no_err=1; }
	elsif (/^no-buffer$/)	{ $no_buffer=1; }
	elsif (/^no-bio$/)	{ $no_bio=1; }
	#elsif (/^no-locking$/)	{ $no_locking=1; }
	elsif (/^no-comp$/)	{ $no_comp=1; }
	elsif (/^no-dso$/)	{ $no_dso=1; }
	elsif (/^no-krb5$/)	{ $no_krb5=1; }
	elsif (/^no-engine$/)	{ $no_engine=1; }
	elsif (/^no-hw$/)	{ $no_hw=1; }
	elsif (/^no-gmp$/)	{ $no_gmp=1; }
	elsif (/^no-rfc3779$/)	{ $no_rfc3779=1; }
	elsif (/^no-tlsext$/)	{ $no_tlsext=1; }
	elsif (/^no-cms$/)	{ $no_cms=1; }
	elsif (/^no-ec2m$/)	{ $no_ec2m=1; }
	elsif (/^no-ec_nistp_64_gcc_128$/)	{ $no_nistp_gcc=1; }
	elsif (/^no-nextprotoneg$/)	{ $no_nextprotoneg=1; }
	elsif (/^no-ssl2$/)	{ $no_ssl2=1; }
	elsif (/^no-ssl2-method$/) { $no_ssl2_method=1; }
	elsif (/^no-ssl3-method$/) { $no_ssl3_method=1; }
	elsif (/^no-capieng$/)	{ $no_capieng=1; }
	elsif (/^no-jpake$/)	{ $no_jpake=1; }
	elsif (/^no-srp$/)	{ $no_srp=1; }
	elsif (/^no-sctp$/)	{ $no_sctp=1; }
	elsif (/^no-srtp$/)	{ $no_srtp=1; }
	elsif (/^no-unit-test$/){ $no_unit_test=1; }
	}


if (!$libname) { 
	if ($do_ssl) {
		$libname="SSLEAY";
	}
	if ($do_crypto) {
		$libname="LIBEAY";
	}
}

# If no platform is given, assume WIN32
if ($W32 + $W16 + $VMS + $OS2 == 0) {
	$W32 = 1;
}

# Add extra knowledge
if ($W16) {
	$no_fp_api=1;
}

if (!$do_ssl && !$do_crypto)
	{
	print STDERR "usage: $0 ( ssl | crypto ) [ 16 | 32 | NT | OS2 ]\n";
	exit(1);
	}

%ssl_list=&load_numbers($ssl_num);
$max_ssl = $max_num;
%crypto_list=&load_numbers($crypto_num);
$max_crypto = $max_num;

my $ssl="ssl/ssl.h";
$ssl.=" ssl/kssl.h";
$ssl.=" ssl/tls1.h";
$ssl.=" ssl/srtp.h";

my $crypto ="crypto/crypto.h";
$crypto.=" crypto/cryptlib.h";
$crypto.=" crypto/o_dir.h";
$crypto.=" crypto/o_str.h";
$crypto.=" crypto/o_time.h";
$crypto.=" crypto/des/des.h crypto/des/des_old.h" ; # unless $no_des;
$crypto.=" crypto/idea/idea.h" ; # unless $no_idea;
$crypto.=" crypto/rc4/rc4.h" ; # unless $no_rc4;
$crypto.=" crypto/rc5/rc5.h" ; # unless $no_rc5;
$crypto.=" crypto/rc2/rc2.h" ; # unless $no_rc2;
$crypto.=" crypto/bf/blowfish.h" ; # unless $no_bf;
$crypto.=" crypto/cast/cast.h" ; # unless $no_cast;
$crypto.=" crypto/whrlpool/whrlpool.h" ;
$crypto.=" crypto/md2/md2.h" ; # unless $no_md2;
$crypto.=" crypto/md4/md4.h" ; # unless $no_md4;
$crypto.=" crypto/md5/md5.h" ; # unless $no_md5;
$crypto.=" crypto/mdc2/mdc2.h" ; # unless $no_mdc2;
$crypto.=" crypto/sha/sha.h" ; # unless $no_sha;
$crypto.=" crypto/ripemd/ripemd.h" ; # unless $no_ripemd;
$crypto.=" crypto/aes/aes.h" ; # unless $no_aes;
$crypto.=" crypto/camellia/camellia.h" ; # unless $no_camellia;
$crypto.=" crypto/seed/seed.h"; # unless $no_seed;

$crypto.=" crypto/bn/bn.h";
$crypto.=" crypto/rsa/rsa.h" ; # unless $no_rsa;
$crypto.=" crypto/dsa/dsa.h" ; # unless $no_dsa;
$crypto.=" crypto/dh/dh.h" ; # unless $no_dh;
$crypto.=" crypto/ec/ec.h" ; # unless $no_ec;
$crypto.=" crypto/ecdsa/ecdsa.h" ; # unless $no_ecdsa;
$crypto.=" crypto/ecdh/ecdh.h" ; # unless $no_ecdh;
$crypto.=" crypto/hmac/hmac.h" ; # unless $no_hmac;
$crypto.=" crypto/cmac/cmac.h" ; # unless $no_hmac;

$crypto.=" crypto/engine/engine.h"; # unless $no_engine;
$crypto.=" crypto/stack/stack.h" ; # unless $no_stack;
$crypto.=" crypto/buffer/buffer.h" ; # unless $no_buffer;
$crypto.=" crypto/bio/bio.h" ; # unless $no_bio;
$crypto.=" crypto/dso/dso.h" ; # unless $no_dso;
$crypto.=" crypto/lhash/lhash.h" ; # unless $no_lhash;
$crypto.=" crypto/conf/conf.h";
$crypto.=" crypto/txt_db/txt_db.h";

$crypto.=" crypto/evp/evp.h" ; # unless $no_evp;
$crypto.=" crypto/objects/objects.h";
$crypto.=" crypto/pem/pem.h";
#$crypto.=" crypto/meth/meth.h";
$crypto.=" crypto/asn1/asn1.h";
$crypto.=" crypto/asn1/asn1t.h";
$crypto.=" crypto/asn1/asn1_mac.h";
$crypto.=" crypto/err/err.h" ; # unless $no_err;
$crypto.=" crypto/pkcs7/pkcs7.h";
$crypto.=" crypto/pkcs12/pkcs12.h";
$crypto.=" crypto/x509/x509.h";
$crypto.=" crypto/x509/x509_vfy.h";
$crypto.=" crypto/x509v3/x509v3.h";
$crypto.=" crypto/ts/ts.h";
$crypto.=" crypto/rand/rand.h";
$crypto.=" crypto/comp/comp.h" ; # unless $no_comp;
$crypto.=" crypto/ocsp/ocsp.h";
$crypto.=" crypto/ui/ui.h crypto/ui/ui_compat.h";
$crypto.=" crypto/krb5/krb5_asn.h";
#$crypto.=" crypto/store/store.h";
$crypto.=" crypto/pqueue/pqueue.h";
$crypto.=" crypto/cms/cms.h";
$crypto.=" crypto/jpake/jpake.h";
$crypto.=" crypto/modes/modes.h";
$crypto.=" crypto/srp/srp.h";

my $symhacks="crypto/symhacks.h";

my @ssl_symbols = &do_defs("SSLEAY", $ssl, $symhacks);
my @crypto_symbols = &do_defs("LIBEAY", $crypto, $symhacks);

if ($do_update) {

if ($do_ssl == 1) {

	&maybe_add_info("SSLEAY",*ssl_list,@ssl_symbols);
	if ($do_rewrite == 1) {
		open(OUT, ">$ssl_num");
		&rewrite_numbers(*OUT,"SSLEAY",*ssl_list,@ssl_symbols);
	} else {
		open(OUT, ">>$ssl_num");
	}
	&update_numbers(*OUT,"SSLEAY",*ssl_list,$max_ssl,@ssl_symbols);
	close OUT;
}

if($do_crypto == 1) {

	&maybe_add_info("LIBEAY",*crypto_list,@crypto_symbols);
	if ($do_rewrite == 1) {
		open(OUT, ">$crypto_num");
		&rewrite_numbers(*OUT,"LIBEAY",*crypto_list,@crypto_symbols);
	} else {
		open(OUT, ">>$crypto_num");
	}
	&update_numbers(*OUT,"LIBEAY",*crypto_list,$max_crypto,@crypto_symbols);
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
	&print_test_file(*STDOUT,"SSLEAY",*ssl_list,$do_ctestall,@ssl_symbols)
		if $do_ssl == 1;

	&print_test_file(*STDOUT,"LIBEAY",*crypto_list,$do_ctestall,@crypto_symbols)
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

	foreach $file (split(/\s+/,$symhacksfile." ".$files))
		{
		print STDERR "DEBUG: starting on $file:\n" if $debug;
		open(IN,"<$file") || die "unable to open $file:$!\n";
		my $line = "", my $def= "";
		my %tag = (
			(map { $_ => 0 } @known_platforms),
			(map { "OPENSSL_SYS_".$_ => 0 } @known_ossl_platforms),
			(map { "OPENSSL_NO_".$_ => 0 } @known_algorithms),
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
				chomp; # remove eol
				chop; # remove ending backslash
				$line = $_;
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
			$cpp = 1 if /^#.*ifdef.*cplusplus/;

			s/{[^{}]*}//gs;                      # ignore {} blocks
			print STDERR "DEBUG: \$def=\"$def\"\n" if $debug && $def ne "";
			print STDERR "DEBUG: \$_=\"$_\"\n" if $debug;
			if (/^\#\s*ifndef\s+(.*)/) {
				push(@tag,"-");
				push(@tag,$1);
				$tag{$1}=-1;
				print STDERR "DEBUG: $file: found tag $1 = -1\n" if $debug;
			} elsif (/^\#\s*if\s+!defined\(([^\)]+)\)/) {
				push(@tag,"-");
				if (/^\#\s*if\s+(!defined\(([^\)]+)\)(\s+\&\&\s+!defined\(([^\)]+)\))*)$/) {
					my $tmp_1 = $1;
					my $tmp_;
					foreach $tmp_ (split '\&\&',$tmp_1) {
						$tmp_ =~ /!defined\(([^\)]+)\)/;
						print STDERR "DEBUG: $file: found tag $1 = -1\n" if $debug;
						push(@tag,$1);
						$tag{$1}=-1;
					}
				} else {
					print STDERR "Warning: $file: complicated expression: $_" if $debug; # because it is O...
					print STDERR "DEBUG: $file: found tag $1 = -1\n" if $debug;
					push(@tag,$1);
					$tag{$1}=-1;
				}
			} elsif (/^\#\s*ifdef\s+(\S*)/) {
				push(@tag,"-");
				push(@tag,$1);
				$tag{$1}=1;
				print STDERR "DEBUG: $file: found tag $1 = 1\n" if $debug;
			} elsif (/^\#\s*if\s+defined\(([^\)]+)\)/) {
				push(@tag,"-");
				if (/^\#\s*if\s+(defined\(([^\)]+)\)(\s+\|\|\s+defined\(([^\)]+)\))*)$/) {
					my $tmp_1 = $1;
					my $tmp_;
					foreach $tmp_ (split '\|\|',$tmp_1) {
						$tmp_ =~ /defined\(([^\)]+)\)/;
						print STDERR "DEBUG: $file: found tag $1 = 1\n" if $debug;
						push(@tag,$1);
						$tag{$1}=1;
					}
				} else {
					print STDERR "Warning: $file: complicated expression: $_\n" if $debug; # because it is O...
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
				@current_algorithms =
				    grep(!/^$/,
					 map { $tag{"OPENSSL_NO_".$_} == -1 ? $_ : "" }
					 @known_algorithms);
				$def .=
				    "#INFO:"
					.join(',',@current_platforms).":"
					    .join(',',@current_algorithms).";";
				next;
			}
			if ($tag{'TRUE'} != -1) {
				if (/^\s*DECLARE_STACK_OF\s*\(\s*(\w*)\s*\)/) {
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
					# Things not in Win16
					$def .=
					    "#INFO:"
						.join(',',"!WIN16",@current_platforms).":"
						    .join(',',@current_algorithms).";";
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
					 /^DECLARE_PEM_write_cb\s*\(\s*(\w*)\s*,/ ) {
					# Things not in Win16
					$def .=
					    "#INFO:"
						.join(',',"!WIN16",@current_platforms).":"
						    .join(',',@current_algorithms).";";
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
					# Things not in Win16
					$def .=
					    "#INFO:"
						.join(',',"!WIN16",@current_platforms).":"
						    .join(',',@current_algorithms).";";
					$def .= "int PEM_read_$1(void);";
					$def .=
					    "#INFO:"
						.join(',',@current_platforms).":"
						    .join(',',@current_algorithms).";";
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

			# Reduce argument lists to empty ()
			# fold round brackets recursively: (t(*v)(t),t) -> (t{}{},t) -> {}
			while(/\(.*\)/s) {
				s/\([^\(\)]+\)/\{\}/gs;
				s/\(\s*\*\s*(\w+)\s*\{\}\s*\)/$1/gs;	#(*f{}) -> f
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
			$a .= ",BF" if($s =~ /EVP_bf/);
			$a .= ",CAST" if($s =~ /EVP_cast/);
			$a .= ",DES" if($s =~ /EVP_des/);
			$a .= ",DSA" if($s =~ /EVP_dss/);
			$a .= ",IDEA" if($s =~ /EVP_idea/);
			$a .= ",MD2" if($s =~ /EVP_md2/);
			$a .= ",MD4" if($s =~ /EVP_md4/);
			$a .= ",MD5" if($s =~ /EVP_md5/);
			$a .= ",RC2" if($s =~ /EVP_rc2/);
			$a .= ",RC4" if($s =~ /EVP_rc4/);
			$a .= ",RC5" if($s =~ /EVP_rc5/);
			$a .= ",RIPEMD" if($s =~ /EVP_ripemd/);
			$a .= ",SHA" if($s =~ /EVP_sha/);
			$a .= ",RSA" if($s =~ /EVP_(Open|Seal)(Final|Init)/);
			$a .= ",RSA" if($s =~ /PEM_Seal(Final|Init|Update)/);
			$a .= ",RSA" if($s =~ /RSAPrivateKey/);
			$a .= ",RSA" if($s =~ /SSLv23?_((client|server)_)?method/);

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

	# Prune the returned symbols

        delete $syms{"bn_dump1"};
	$platform{"BIO_s_log"} .= ",!WIN32,!WIN16,!macintosh";

	$platform{"PEM_read_NS_CERT_SEQ"} = "VMS";
	$platform{"PEM_write_NS_CERT_SEQ"} = "VMS";
	$platform{"PEM_read_P8_PRIV_KEY_INFO"} = "VMS";
	$platform{"PEM_write_P8_PRIV_KEY_INFO"} = "VMS";
	$platform{"EVP_sha384"} = "!VMSVAX";
	$platform{"EVP_sha512"} = "!VMSVAX";
	$platform{"SHA384_Init"} = "!VMSVAX";
	$platform{"SHA384_Transform"} = "!VMSVAX";
	$platform{"SHA384_Update"} = "!VMSVAX";
	$platform{"SHA384_Final"} = "!VMSVAX";
	$platform{"SHA384"} = "!VMSVAX";
	$platform{"SHA512_Init"} = "!VMSVAX";
	$platform{"SHA512_Transform"} = "!VMSVAX";
	$platform{"SHA512_Update"} = "!VMSVAX";
	$platform{"SHA512_Final"} = "!VMSVAX";
	$platform{"SHA512"} = "!VMSVAX";
	$platform{"WHIRLPOOL_Init"} = "!VMSVAX";
	$platform{"WHIRLPOOL"} = "!VMSVAX";
	$platform{"WHIRLPOOL_BitUpdate"} = "!VMSVAX";
	$platform{"EVP_whirlpool"} = "!VMSVAX";
	$platform{"WHIRLPOOL_Final"} = "!VMSVAX";
	$platform{"WHIRLPOOL_Update"} = "!VMSVAX";


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

sub info_string {
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

sub maybe_add_info {
	(my $name, *nums, my @symbols) = @_;
	my $sym;
	my $new_info = 0;
	my %syms=();

	print STDERR "Updating $name info\n";
	foreach $sym (@symbols) {
		(my $s, my $i) = split /\\/, $sym;
		if (defined($nums{$s})) {
			$i =~ s/^(.*?:.*?:\w+)(\(\w+\))?/$1/;
			(my $n, my $dummy) = split /\\/, $nums{$s};
			if (!defined($dummy) || $i ne $dummy) {
				$nums{$s} = $n."\\".$i;
				$new_info++;
				print STDERR "DEBUG: maybe_add_info for $s: \"$dummy\" => \"$i\"\n" if $debug;
			}
		}
		$syms{$s} = 1;
	}

	my @s=sort { &parse_number($nums{$a},"n") <=> &parse_number($nums{$b},"n") } keys %nums;
	foreach $sym (@s) {
		(my $n, my $i) = split /\\/, $nums{$sym};
		if (!defined($syms{$sym}) && $i !~ /^NOEXIST:/) {
			$new_info++;
			print STDERR "DEBUG: maybe_add_info for $sym: -> undefined\n" if $debug;
		}
	}
	if ($new_info) {
		print STDERR "$new_info old symbols got an info update\n";
		if (!$do_rewrite) {
			print STDERR "You should do a rewrite to fix this.\n";
		}
	} else {
		print STDERR "No old symbols needed info update\n";
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
			if ($keyword eq "VMSVAX" && $VMSVAX) { return 1; }
			if ($keyword eq "VMSNonVAX" && $VMSNonVAX) { return 1; }
			if ($keyword eq "VMS" && $VMS) { return 1; }
			if ($keyword eq "WIN32" && $W32) { return 1; }
			if ($keyword eq "WIN16" && $W16) { return 1; }
			if ($keyword eq "WINNT" && $NT) { return 1; }
			if ($keyword eq "OS2" && $OS2) { return 1; }
			# Special platforms:
			# EXPORT_VAR_AS_FUNCTION means that global variables
			# will be represented as functions.  This currently
			# only happens on VMS-VAX.
			if ($keyword eq "EXPORT_VAR_AS_FUNCTION" && ($VMSVAX || $W32 || $W16)) {
				return 1;
			}
			if ($keyword eq "OPENSSL_FIPS" && $fips) {
				return 1;
			}
			if ($keyword eq "ZLIB" && $zlib) { return 1; }
			return 0;
		} else {
			# algorithms
			if ($keyword eq "RC2" && $no_rc2) { return 0; }
			if ($keyword eq "RC4" && $no_rc4) { return 0; }
			if ($keyword eq "RC5" && $no_rc5) { return 0; }
			if ($keyword eq "IDEA" && $no_idea) { return 0; }
			if ($keyword eq "DES" && $no_des) { return 0; }
			if ($keyword eq "BF" && $no_bf) { return 0; }
			if ($keyword eq "CAST" && $no_cast) { return 0; }
			if ($keyword eq "MD2" && $no_md2) { return 0; }
			if ($keyword eq "MD4" && $no_md4) { return 0; }
			if ($keyword eq "MD5" && $no_md5) { return 0; }
			if ($keyword eq "SHA" && $no_sha) { return 0; }
			if ($keyword eq "RIPEMD" && $no_ripemd) { return 0; }
			if ($keyword eq "MDC2" && $no_mdc2) { return 0; }
			if ($keyword eq "WHIRLPOOL" && $no_whirlpool) { return 0; }
			if ($keyword eq "RSA" && $no_rsa) { return 0; }
			if ($keyword eq "DSA" && $no_dsa) { return 0; }
			if ($keyword eq "DH" && $no_dh) { return 0; }
			if ($keyword eq "EC" && $no_ec) { return 0; }
			if ($keyword eq "ECDSA" && $no_ecdsa) { return 0; }
			if ($keyword eq "ECDH" && $no_ecdh) { return 0; }
			if ($keyword eq "HMAC" && $no_hmac) { return 0; }
			if ($keyword eq "AES" && $no_aes) { return 0; }
			if ($keyword eq "CAMELLIA" && $no_camellia) { return 0; }
			if ($keyword eq "SEED" && $no_seed) { return 0; }
			if ($keyword eq "EVP" && $no_evp) { return 0; }
			if ($keyword eq "LHASH" && $no_lhash) { return 0; }
			if ($keyword eq "STACK" && $no_stack) { return 0; }
			if ($keyword eq "ERR" && $no_err) { return 0; }
			if ($keyword eq "BUFFER" && $no_buffer) { return 0; }
			if ($keyword eq "BIO" && $no_bio) { return 0; }
			if ($keyword eq "COMP" && $no_comp) { return 0; }
			if ($keyword eq "DSO" && $no_dso) { return 0; }
			if ($keyword eq "KRB5" && $no_krb5) { return 0; }
			if ($keyword eq "ENGINE" && $no_engine) { return 0; }
			if ($keyword eq "HW" && $no_hw) { return 0; }
			if ($keyword eq "FP_API" && $no_fp_api) { return 0; }
			if ($keyword eq "STATIC_ENGINE" && $no_static_engine) { return 0; }
			if ($keyword eq "GMP" && $no_gmp) { return 0; }
			if ($keyword eq "RFC3779" && $no_rfc3779) { return 0; }
			if ($keyword eq "TLSEXT" && $no_tlsext) { return 0; }
			if ($keyword eq "PSK" && $no_psk) { return 0; }
			if ($keyword eq "CMS" && $no_cms) { return 0; }
			if ($keyword eq "EC2M" && $no_ec2m) { return 0; }
			if ($keyword eq "NEXTPROTONEG" && $no_nextprotoneg) { return 0; }
			if ($keyword eq "EC_NISTP_64_GCC_128" && $no_nistp_gcc)
					{ return 0; }
			if ($keyword eq "SSL2" && $no_ssl2) { return 0; }
			if ($keyword eq "SSL2_METHOD" && $no_ssl2_method) { return 0; }
			if ($keyword eq "SSL3_METHOD" && $no_ssl3_method) { return 0; }
			if ($keyword eq "CAPIENG" && $no_capieng) { return 0; }
			if ($keyword eq "JPAKE" && $no_jpake) { return 0; }
			if ($keyword eq "SRP" && $no_srp) { return 0; }
			if ($keyword eq "SCTP" && $no_sctp) { return 0; }
			if ($keyword eq "SRTP" && $no_srtp) { return 0; }
			if ($keyword eq "UNIT_TEST" && $no_unit_test) { return 0; }
			if ($keyword eq "DEPRECATED" && $no_deprecated) { return 0; }

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

			($nn,$ni)=($nums{$s2} =~ /^(.*?)\\(.*)$/);
			if ($v) {
				print OUT "\textern int $s2; /* type unknown */ /* $nn $ni */\n";
			} else {
				print OUT "\textern int $s2(); /* type unknown */ /* $nn $ni */\n";
			}
		}
	}
}

sub get_version {
   local *MF;
   my $v = '?';
   open MF, 'Makefile' or return $v;
   while (<MF>) {
     $v = $1, last if /^VERSION=(.*?)\s*$/;
   }
   close MF;
   return $v;
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

	if ($W32)
		{ $libname.="32"; }
	elsif ($W16)
		{ $libname.="16"; }
	elsif ($OS2)
		{ # DLL names should not clash on the whole system.
		  # However, they should not have any particular relationship
		  # to the name of the static library.  Chose descriptive names
		  # (must be at most 8 chars).
		  my %translate = (ssl => 'open_ssl', crypto => 'cryptssl');
		  $libname = $translate{$name} || $name;
		  $liboptions = <<EOO;
INITINSTANCE
DATA MULTIPLE NONSHARED
EOO
		  # Vendor field can't contain colon, drat; so we omit http://
		  $description = "\@#$http_vendor:$version#\@$what; DLL for library $name.  Build for EMX -Zmtd";
		}

	print OUT <<"EOF";
;
; Definition file for the DLL version of the $name library from OpenSSL
;

LIBRARY         $libname	$liboptions

EOF

	if ($W16) {
		print <<"EOF";
CODE            PRELOAD MOVEABLE
DATA            PRELOAD MOVEABLE SINGLE

EXETYPE		WINDOWS

HEAPSIZE	4096
STACKSIZE	8192

EOF
	}

	print "EXPORTS\n";

	(@e)=grep(/^SSLeay(\{[0-9]+\})?\\.*?:.*?:FUNCTION/,@symbols);
	(@r)=grep(/^\w+(\{[0-9]+\})?\\.*?:.*?:FUNCTION/ && !/^SSLeay(\{[0-9]+\})?\\.*?:.*?:FUNCTION/,@symbols);
	(@v)=grep(/^\w+(\{[0-9]+\})?\\.*?:.*?:VARIABLE/,@symbols);
	@symbols=((sort @e),(sort @r), (sort @v));


	foreach $sym (@symbols) {
		(my $s, my $i) = $sym =~ /^(.*?)\\(.*)$/;
		my $v = 0;
		$v = 1 if $i =~ /^.*?:.*?:VARIABLE/;
		if (!defined($nums{$s})) {
			printf STDERR "Warning: $s does not have a number assigned\n"
			    if(!$do_update);
		} else {
			(my $n, my $dummy) = split /\\/, $nums{$s};
			my %pf = ();
			my $p = ($i =~ /^[^:]*:([^:]*):/,$1);
			my $a = ($i =~ /^[^:]*:[^:]*:[^:]*:([^:]*)/,$1);
			if (is_valid($p,1) && is_valid($a,0)) {
				my $s2 = ($s =~ /^(.*?)(\{[0-9]+\})?$/, $1);
				if ($prev eq $s2) {
					print STDERR "Warning: Symbol '",$s2,"' redefined. old=",($nums{$prev} =~ /^(.*?)\\/,$1),", new=",($nums{$s2} =~ /^(.*?)\\/,$1),"\n";
				}
				$prev = $s2;	# To warn about duplicates...
				if($v && !$OS2) {
					printf OUT "    %s%-39s @%-8d DATA\n",($W32)?"":"_",$s2,$n;
				} else {
					printf OUT "    %s%-39s @%d\n",($W32||$OS2)?"":"_",$s2,$n;
				}
			}
		}
	}
	printf OUT "\n";
}

sub load_numbers
{
	my($name)=@_;
	my(@a,%ret);

	$max_num = 0;
	$num_noinfo = 0;
	$prev = "";
	$prev_cnt = 0;

	open(IN,"<$name") || die "unable to open $name:$!\n";
	while (<IN>) {
		chop;
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
			$ret{$a[0]}=$a[1]."\\".$a[2]; # \\ is a special marker
		}
		$max_num = $a[1] if $a[1] > $max_num;
		$prev=$a[0];
	}
	if ($num_noinfo) {
		print STDERR "Warning: $num_noinfo symbols were without info.";
		if ($do_rewrite) {
			printf STDERR "  The rewrite will fix this.\n";
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
	(my $n, my $i) = split(/\\/,$str);
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

	print STDERR "Rewriting $name\n";

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
		(my $n, my $i) = split /\\/, $nums{$sym};
		next if defined($i) && $i =~ /^.*?:.*?:\w+\(\w+\)/;
		next if defined($rsyms{$sym});
		print STDERR "DEBUG: rewrite_numbers for sym = ",$sym,": i = ",$i,", n = ",$n,", rsym{sym} = ",$rsyms{$sym},"syms{sym} = ",$syms{$sym},"\n" if $debug;
		$i="NOEXIST::FUNCTION:"
			if !defined($i) || $i eq "" || !defined($syms{$sym});
		my $s2 = $sym;
		$s2 =~ s/\{[0-9]+\}$//;
		printf OUT "%s%-39s %d\t%s\n","",$s2,$n,$i;
		if (exists $r{$sym}) {
			(my $s, $i) = split /\\/,$r{$sym};
			my $s2 = $s;
			$s2 =~ s/\{[0-9]+\}$//;
			printf OUT "%s%-39s %d\t%s\n","",$s2,$n,$i;
		}
	}
}

sub update_numbers
{
	(*OUT,$name,*nums,my $start_num, my @symbols)=@_;
	my $new_syms = 0;

	print STDERR "Updating $name numbers\n";

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
			printf OUT "%s%-39s %d\t%s\n","",$s2, ++$start_num,$i;
			if (exists $r{$s}) {
				($s, $i) = split /\\/,$r{$s};
				$s =~ s/\{[0-9]+\}$//;
				printf OUT "%s%-39s %d\t%s\n","",$s, $start_num,$i;
			}
		}
	}
	if($new_syms) {
		print STDERR "$new_syms New symbols added\n";
	} else {
		print STDERR "No New symbols Added\n";
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

