#!/usr/bin/perl -w
# Perl utility to run or verify FIPS 140-2 CMVP algorithm tests based on the
# pathnames of input algorithm test files actually present (the unqualified
# file names are consistent but the pathnames are not).
#

# FIPS test definitions
# List of all the unqualified file names we expect and command lines to run

# DSA tests
my @fips_dsa_test_list = (

    "DSA",

    [ "PQGGen",  "fips_dssvs pqg" ],
    [ "KeyPair", "fips_dssvs keypair" ],
    [ "SigGen",  "fips_dssvs siggen" ],
    [ "SigVer",  "fips_dssvs sigver" ]

);

my @fips_dsa_pqgver_test_list = (

    [ "PQGVer",  "fips_dssvs pqgver" ]

);

# RSA tests

my @fips_rsa_test_list = (

    "RSA",

    [ "SigGen15",  "fips_rsastest" ],
    [ "SigVer15",  "fips_rsavtest" ],
    [ "SigVerRSA", "fips_rsavtest -x931" ],
    [ "KeyGenRSA", "fips_rsagtest" ],
    [ "SigGenRSA", "fips_rsastest -x931" ]

);

# Special cases for PSS. The filename itself is
# not sufficient to determine the test. Addditionally we
# need to examine the file contents to determine the salt length
# In these cases the test filename has (saltlen) appended.

# RSA PSS salt length 0 tests

my @fips_rsa_pss0_test_list = (

    [ "SigGenPSS(0)", "fips_rsastest -saltlen 0" ],
    [ "SigVerPSS(0)", "fips_rsavtest -saltlen 0" ]

);

# RSA PSS salt length 62 tests

my @fips_rsa_pss62_test_list = (
    [ "SigGenPSS(62)", "fips_rsastest -saltlen 62" ],
    [ "SigVerPSS(62)", "fips_rsavtest -saltlen 62" ]

);

# SHA tests

my @fips_sha_test_list = (

    "SHA",

    [ "SHA1LongMsg",    "fips_shatest" ],
    [ "SHA1Monte",      "fips_shatest" ],
    [ "SHA1ShortMsg",   "fips_shatest" ],
    [ "SHA224LongMsg",  "fips_shatest" ],
    [ "SHA224Monte",    "fips_shatest" ],
    [ "SHA224ShortMsg", "fips_shatest" ],
    [ "SHA256LongMsg",  "fips_shatest" ],
    [ "SHA256Monte",    "fips_shatest" ],
    [ "SHA256ShortMsg", "fips_shatest" ],
    [ "SHA384LongMsg",  "fips_shatest" ],
    [ "SHA384Monte",    "fips_shatest" ],
    [ "SHA384ShortMsg", "fips_shatest" ],
    [ "SHA512LongMsg",  "fips_shatest" ],
    [ "SHA512Monte",    "fips_shatest" ],
    [ "SHA512ShortMsg", "fips_shatest" ]

);

# HMAC

my @fips_hmac_test_list = (

    "HMAC",

    [ "HMAC", "fips_hmactest" ]

);

# RAND tests, AES version

my @fips_rand_aes_test_list = (

    "RAND (AES)",

    [ "ANSI931_AES128MCT", "fips_rngvs mct" ],
    [ "ANSI931_AES192MCT", "fips_rngvs mct" ],
    [ "ANSI931_AES256MCT", "fips_rngvs mct" ],
    [ "ANSI931_AES128VST", "fips_rngvs vst" ],
    [ "ANSI931_AES192VST", "fips_rngvs vst" ],
    [ "ANSI931_AES256VST", "fips_rngvs vst" ]

);

# RAND tests, DES2 version

my @fips_rand_des2_test_list = (

    "RAND (DES2)",

    [ "ANSI931_TDES2MCT", "fips_rngvs mct" ],
    [ "ANSI931_TDES2VST", "fips_rngvs vst" ]

);

# AES tests

my @fips_aes_test_list = (

    "AES",

    [ "CBCGFSbox128",     "fips_aesavs -f" ],
    [ "CBCGFSbox192",     "fips_aesavs -f" ],
    [ "CBCGFSbox256",     "fips_aesavs -f" ],
    [ "CBCKeySbox128",    "fips_aesavs -f" ],
    [ "CBCKeySbox192",    "fips_aesavs -f" ],
    [ "CBCKeySbox256",    "fips_aesavs -f" ],
    [ "CBCMCT128",        "fips_aesavs -f" ],
    [ "CBCMCT192",        "fips_aesavs -f" ],
    [ "CBCMCT256",        "fips_aesavs -f" ],
    [ "CBCMMT128",        "fips_aesavs -f" ],
    [ "CBCMMT192",        "fips_aesavs -f" ],
    [ "CBCMMT256",        "fips_aesavs -f" ],
    [ "CBCVarKey128",     "fips_aesavs -f" ],
    [ "CBCVarKey192",     "fips_aesavs -f" ],
    [ "CBCVarKey256",     "fips_aesavs -f" ],
    [ "CBCVarTxt128",     "fips_aesavs -f" ],
    [ "CBCVarTxt192",     "fips_aesavs -f" ],
    [ "CBCVarTxt256",     "fips_aesavs -f" ],
    [ "CFB128GFSbox128",  "fips_aesavs -f" ],
    [ "CFB128GFSbox192",  "fips_aesavs -f" ],
    [ "CFB128GFSbox256",  "fips_aesavs -f" ],
    [ "CFB128KeySbox128", "fips_aesavs -f" ],
    [ "CFB128KeySbox192", "fips_aesavs -f" ],
    [ "CFB128KeySbox256", "fips_aesavs -f" ],
    [ "CFB128MCT128",     "fips_aesavs -f" ],
    [ "CFB128MCT192",     "fips_aesavs -f" ],
    [ "CFB128MCT256",     "fips_aesavs -f" ],
    [ "CFB128MMT128",     "fips_aesavs -f" ],
    [ "CFB128MMT192",     "fips_aesavs -f" ],
    [ "CFB128MMT256",     "fips_aesavs -f" ],
    [ "CFB128VarKey128",  "fips_aesavs -f" ],
    [ "CFB128VarKey192",  "fips_aesavs -f" ],
    [ "CFB128VarKey256",  "fips_aesavs -f" ],
    [ "CFB128VarTxt128",  "fips_aesavs -f" ],
    [ "CFB128VarTxt192",  "fips_aesavs -f" ],
    [ "CFB128VarTxt256",  "fips_aesavs -f" ],
    [ "CFB8GFSbox128",    "fips_aesavs -f" ],
    [ "CFB8GFSbox192",    "fips_aesavs -f" ],
    [ "CFB8GFSbox256",    "fips_aesavs -f" ],
    [ "CFB8KeySbox128",   "fips_aesavs -f" ],
    [ "CFB8KeySbox192",   "fips_aesavs -f" ],
    [ "CFB8KeySbox256",   "fips_aesavs -f" ],
    [ "CFB8MCT128",       "fips_aesavs -f" ],
    [ "CFB8MCT192",       "fips_aesavs -f" ],
    [ "CFB8MCT256",       "fips_aesavs -f" ],
    [ "CFB8MMT128",       "fips_aesavs -f" ],
    [ "CFB8MMT192",       "fips_aesavs -f" ],
    [ "CFB8MMT256",       "fips_aesavs -f" ],
    [ "CFB8VarKey128",    "fips_aesavs -f" ],
    [ "CFB8VarKey192",    "fips_aesavs -f" ],
    [ "CFB8VarKey256",    "fips_aesavs -f" ],
    [ "CFB8VarTxt128",    "fips_aesavs -f" ],
    [ "CFB8VarTxt192",    "fips_aesavs -f" ],
    [ "CFB8VarTxt256",    "fips_aesavs -f" ],

    [ "ECBGFSbox128",  "fips_aesavs -f" ],
    [ "ECBGFSbox192",  "fips_aesavs -f" ],
    [ "ECBGFSbox256",  "fips_aesavs -f" ],
    [ "ECBKeySbox128", "fips_aesavs -f" ],
    [ "ECBKeySbox192", "fips_aesavs -f" ],
    [ "ECBKeySbox256", "fips_aesavs -f" ],
    [ "ECBMCT128",     "fips_aesavs -f" ],
    [ "ECBMCT192",     "fips_aesavs -f" ],
    [ "ECBMCT256",     "fips_aesavs -f" ],
    [ "ECBMMT128",     "fips_aesavs -f" ],
    [ "ECBMMT192",     "fips_aesavs -f" ],
    [ "ECBMMT256",     "fips_aesavs -f" ],
    [ "ECBVarKey128",  "fips_aesavs -f" ],
    [ "ECBVarKey192",  "fips_aesavs -f" ],
    [ "ECBVarKey256",  "fips_aesavs -f" ],
    [ "ECBVarTxt128",  "fips_aesavs -f" ],
    [ "ECBVarTxt192",  "fips_aesavs -f" ],
    [ "ECBVarTxt256",  "fips_aesavs -f" ],
    [ "OFBGFSbox128",  "fips_aesavs -f" ],
    [ "OFBGFSbox192",  "fips_aesavs -f" ],
    [ "OFBGFSbox256",  "fips_aesavs -f" ],
    [ "OFBKeySbox128", "fips_aesavs -f" ],
    [ "OFBKeySbox192", "fips_aesavs -f" ],
    [ "OFBKeySbox256", "fips_aesavs -f" ],
    [ "OFBMCT128",     "fips_aesavs -f" ],
    [ "OFBMCT192",     "fips_aesavs -f" ],
    [ "OFBMCT256",     "fips_aesavs -f" ],
    [ "OFBMMT128",     "fips_aesavs -f" ],
    [ "OFBMMT192",     "fips_aesavs -f" ],
    [ "OFBMMT256",     "fips_aesavs -f" ],
    [ "OFBVarKey128",  "fips_aesavs -f" ],
    [ "OFBVarKey192",  "fips_aesavs -f" ],
    [ "OFBVarKey256",  "fips_aesavs -f" ],
    [ "OFBVarTxt128",  "fips_aesavs -f" ],
    [ "OFBVarTxt192",  "fips_aesavs -f" ],
    [ "OFBVarTxt256",  "fips_aesavs -f" ]

);

my @fips_aes_cfb1_test_list = (

    # AES CFB1 tests

    [ "CFB1GFSbox128",  "fips_aesavs -f" ],
    [ "CFB1GFSbox192",  "fips_aesavs -f" ],
    [ "CFB1GFSbox256",  "fips_aesavs -f" ],
    [ "CFB1KeySbox128", "fips_aesavs -f" ],
    [ "CFB1KeySbox192", "fips_aesavs -f" ],
    [ "CFB1KeySbox256", "fips_aesavs -f" ],
    [ "CFB1MCT128",     "fips_aesavs -f" ],
    [ "CFB1MCT192",     "fips_aesavs -f" ],
    [ "CFB1MCT256",     "fips_aesavs -f" ],
    [ "CFB1MMT128",     "fips_aesavs -f" ],
    [ "CFB1MMT192",     "fips_aesavs -f" ],
    [ "CFB1MMT256",     "fips_aesavs -f" ],
    [ "CFB1VarKey128",  "fips_aesavs -f" ],
    [ "CFB1VarKey192",  "fips_aesavs -f" ],
    [ "CFB1VarKey256",  "fips_aesavs -f" ],
    [ "CFB1VarTxt128",  "fips_aesavs -f" ],
    [ "CFB1VarTxt192",  "fips_aesavs -f" ],
    [ "CFB1VarTxt256",  "fips_aesavs -f" ]

);

# Triple DES tests

my @fips_des3_test_list = (

    "Triple DES",

    [ "TCBCinvperm",   "fips_desmovs -f" ],
    [ "TCBCMMT1",      "fips_desmovs -f" ],
    [ "TCBCMMT2",      "fips_desmovs -f" ],
    [ "TCBCMMT3",      "fips_desmovs -f" ],
    [ "TCBCMonte1",    "fips_desmovs -f" ],
    [ "TCBCMonte2",    "fips_desmovs -f" ],
    [ "TCBCMonte3",    "fips_desmovs -f" ],
    [ "TCBCpermop",    "fips_desmovs -f" ],
    [ "TCBCsubtab",    "fips_desmovs -f" ],
    [ "TCBCvarkey",    "fips_desmovs -f" ],
    [ "TCBCvartext",   "fips_desmovs -f" ],
    [ "TCFB64invperm", "fips_desmovs -f" ],
    [ "TCFB64MMT1",    "fips_desmovs -f" ],
    [ "TCFB64MMT2",    "fips_desmovs -f" ],
    [ "TCFB64MMT3",    "fips_desmovs -f" ],
    [ "TCFB64Monte1",  "fips_desmovs -f" ],
    [ "TCFB64Monte2",  "fips_desmovs -f" ],
    [ "TCFB64Monte3",  "fips_desmovs -f" ],
    [ "TCFB64permop",  "fips_desmovs -f" ],
    [ "TCFB64subtab",  "fips_desmovs -f" ],
    [ "TCFB64varkey",  "fips_desmovs -f" ],
    [ "TCFB64vartext", "fips_desmovs -f" ],
    [ "TCFB8invperm",  "fips_desmovs -f" ],
    [ "TCFB8MMT1",     "fips_desmovs -f" ],
    [ "TCFB8MMT2",     "fips_desmovs -f" ],
    [ "TCFB8MMT3",     "fips_desmovs -f" ],
    [ "TCFB8Monte1",   "fips_desmovs -f" ],
    [ "TCFB8Monte2",   "fips_desmovs -f" ],
    [ "TCFB8Monte3",   "fips_desmovs -f" ],
    [ "TCFB8permop",   "fips_desmovs -f" ],
    [ "TCFB8subtab",   "fips_desmovs -f" ],
    [ "TCFB8varkey",   "fips_desmovs -f" ],
    [ "TCFB8vartext",  "fips_desmovs -f" ],
    [ "TECBinvperm",   "fips_desmovs -f" ],
    [ "TECBMMT1",      "fips_desmovs -f" ],
    [ "TECBMMT2",      "fips_desmovs -f" ],
    [ "TECBMMT3",      "fips_desmovs -f" ],
    [ "TECBMonte1",    "fips_desmovs -f" ],
    [ "TECBMonte2",    "fips_desmovs -f" ],
    [ "TECBMonte3",    "fips_desmovs -f" ],
    [ "TECBpermop",    "fips_desmovs -f" ],
    [ "TECBsubtab",    "fips_desmovs -f" ],
    [ "TECBvarkey",    "fips_desmovs -f" ],
    [ "TECBvartext",   "fips_desmovs -f" ],
    [ "TOFBinvperm",   "fips_desmovs -f" ],
    [ "TOFBMMT1",      "fips_desmovs -f" ],
    [ "TOFBMMT2",      "fips_desmovs -f" ],
    [ "TOFBMMT3",      "fips_desmovs -f" ],
    [ "TOFBMonte1",    "fips_desmovs -f" ],
    [ "TOFBMonte2",    "fips_desmovs -f" ],
    [ "TOFBMonte3",    "fips_desmovs -f" ],
    [ "TOFBpermop",    "fips_desmovs -f" ],
    [ "TOFBsubtab",    "fips_desmovs -f" ],
    [ "TOFBvarkey",    "fips_desmovs -f" ],
    [ "TOFBvartext",   "fips_desmovs -f" ]

);

my @fips_des3_cfb1_test_list = (

    # DES3 CFB1 tests

    [ "TCFB1invperm",  "fips_desmovs -f" ],
    [ "TCFB1MMT1",     "fips_desmovs -f" ],
    [ "TCFB1MMT2",     "fips_desmovs -f" ],
    [ "TCFB1MMT3",     "fips_desmovs -f" ],
    [ "TCFB1Monte1",   "fips_desmovs -f" ],
    [ "TCFB1Monte2",   "fips_desmovs -f" ],
    [ "TCFB1Monte3",   "fips_desmovs -f" ],
    [ "TCFB1permop",   "fips_desmovs -f" ],
    [ "TCFB1subtab",   "fips_desmovs -f" ],
    [ "TCFB1varkey",   "fips_desmovs -f" ],
    [ "TCFB1vartext",  "fips_desmovs -f" ],

);

# Verification special cases.
# In most cases the output of a test is deterministic and
# it can be compared to a known good result. A few involve
# the genration and use of random keys and the output will
# be different each time. In thoses cases we perform special tests
# to simply check their consistency. For example signature generation
# output will be run through signature verification to see if all outputs
# show as valid.
#

my %verify_special = (
    "PQGGen"        => "fips_dssvs pqgver",
    "KeyPair"       => "fips_dssvs keyver",
    "SigGen"        => "fips_dssvs sigver",
    "SigGen15"      => "fips_rsavtest",
    "SigGenRSA"     => "fips_rsavtest -x931",
    "SigGenPSS(0)"  => "fips_rsavtest -saltlen 0",
    "SigGenPSS(62)" => "fips_rsavtest -saltlen 62",
);

my $win32  = $^O =~ m/mswin/i;
my $onedir = 0;
my $filter = "";
my $tvdir;
my $tprefix;
my $shwrap_prefix;
my $debug          = 0;
my $quiet          = 0;
my $notest         = 0;
my $verify         = 1;
my $rspdir         = "rsp";
my $ignore_missing = 0;
my $ignore_bogus   = 0;
my $bufout         = '';
my $list_tests     = 0;

my %fips_enabled = (
    dsa         => 1,
    "dsa-pqgver"  => 0,
    rsa         => 1,
    "rsa-pss0"  => 0,
    "rsa-pss62" => 1,
    sha         => 1,
    hmac        => 1,
    "rand-aes"  => 1,
    "rand-des2" => 0,
    aes         => 1,
    "aes-cfb1"  => 0,
    des3        => 1,
    "des3-cfb1" => 0
);

foreach (@ARGV) {
    if ( $_ eq "--win32" ) {
        $win32 = 1;
    }
    elsif ( $_ eq "--onedir" ) {
        $onedir = 1;
    }
    elsif ( $_ eq "--debug" ) {
        $debug = 1;
    }
    elsif ( $_ eq "--ignore-missing" ) {
        $ignore_missing = 1;
    }
    elsif ( $_ eq "--ignore-bogus" ) {
        $ignore_bogus = 1;
    }
    elsif ( $_ eq "--generate" ) {
        $verify = 0;
    }
    elsif ( $_ eq "--notest" ) {
        $notest = 1;
    }
    elsif ( $_ eq "--quiet" ) {
        $quiet = 1;
    }
    elsif (/--dir=(.*)$/) {
        $tvdir = $1;
    }
    elsif (/--rspdir=(.*)$/) {
        $rspdir = $1;
    }
    elsif (/--tprefix=(.*)$/) {
        $tprefix = $1;
    }
    elsif (/--shwrap_prefix=(.*)$/) {
        $shwrap_prefix = $1;
    }
    elsif (/^--(enable|disable)-(.*)$/) {
        if ( !exists $fips_enabled{$2} ) {
            print STDERR "Unknown test $2\n";
        }
        if ( $1 eq "enable" ) {
            $fips_enabled{$2} = 1;
        }
        else {
            $fips_enabled{$2} = 0;
        }
    }
    elsif (/--filter=(.*)$/) {
        $filter = $1;
    }
    elsif (/^--list-tests$/) {
        $list_tests = 1;
    }
    else {
        Help();
        exit(1);
    }
}

my @fips_test_list;

push @fips_test_list, @fips_dsa_test_list       if $fips_enabled{"dsa"};
push @fips_test_list, @fips_dsa_pqgver_test_list if $fips_enabled{"dsa-pqgver"};
push @fips_test_list, @fips_rsa_test_list       if $fips_enabled{"rsa"};
push @fips_test_list, @fips_rsa_pss0_test_list  if $fips_enabled{"rsa-pss0"};
push @fips_test_list, @fips_rsa_pss62_test_list if $fips_enabled{"rsa-pss62"};
push @fips_test_list, @fips_sha_test_list       if $fips_enabled{"sha"};
push @fips_test_list, @fips_hmac_test_list      if $fips_enabled{"hmac"};
push @fips_test_list, @fips_rand_aes_test_list  if $fips_enabled{"rand-aes"};
push @fips_test_list, @fips_rand_des2_test_list if $fips_enabled{"rand-des2"};
push @fips_test_list, @fips_aes_test_list       if $fips_enabled{"aes"};
push @fips_test_list, @fips_aes_cfb1_test_list  if $fips_enabled{"aes-cfb1"};
push @fips_test_list, @fips_des3_test_list      if $fips_enabled{"des3"};
push @fips_test_list, @fips_des3_cfb1_test_list if $fips_enabled{"des3-cfb1"};

if ($list_tests) {
    my ( $test, $en );
    print "=====TEST LIST=====\n";
    foreach $test ( sort keys %fips_enabled ) {
        $en = $fips_enabled{$test};
        $test =~ tr/[a-z]/[A-Z]/;
        printf "%-10s %s\n", $test, $en ? "enabled" : "disabled";
    }
    exit(0);
}

foreach (@fips_test_list) {
    next unless ref($_);
    my $nm = $_->[0];
    $_->[2] = "";
    $_->[3] = "";
    print STDERR "Duplicate test $nm\n" if exists $fips_tests{$nm};
    $fips_tests{$nm} = $_;
}

$tvdir = "." unless defined $tvdir;

if ($win32) {
    if ( !defined $tprefix ) {
        if ($onedir) {
            $tprefix = ".\\";
        }
        else {
            $tprefix = "..\\out32dll\\";
        }
    }
}
else {
    if ($onedir) {
        $tprefix       = "./" unless defined $tprefix;
        $shwrap_prefix = "./" unless defined $shwrap_prefix;
    }
    else {
        $tprefix       = "../test/" unless defined $tprefix;
        $shwrap_prefix = "../util/" unless defined $shwrap_prefix;
    }
}

sanity_check_exe( $win32, $tprefix, $shwrap_prefix );

my $cmd_prefix = $win32 ? "" : "${shwrap_prefix}shlib_wrap.sh ";

find_files( $filter, $tvdir );

sanity_check_files();

my ( $runerr, $cmperr, $cmpok, $scheckrunerr, $scheckerr, $scheckok, $skipcnt )
  = ( 0, 0, 0, 0, 0, 0, 0 );

exit(0) if $notest;

run_tests( $verify, $win32, $tprefix, $filter, $tvdir );

if ($verify) {
    print "ALGORITHM TEST VERIFY SUMMARY REPORT:\n";
    print "Tests skipped due to missing files:        $skipcnt\n";
    print "Algorithm test program execution failures: $runerr\n";
    print "Test comparisons successful:               $cmpok\n";
    print "Test comparisons failed:                   $cmperr\n";
    print "Test sanity checks successful:             $scheckok\n";
    print "Test sanity checks failed:                 $scheckerr\n";
    print "Sanity check program execution failures:   $scheckrunerr\n";

    if ( $runerr || $cmperr || $scheckrunerr || $scheckerr ) {
        print "***TEST FAILURE***\n";
    }
    else {
        print "***ALL TESTS SUCCESSFUL***\n";
    }
}
else {
    print "ALGORITHM TEST SUMMARY REPORT:\n";
    print "Tests skipped due to missing files:        $skipcnt\n";
    print "Algorithm test program execution failures: $runerr\n";

    if ($runerr) {
        print "***TEST FAILURE***\n";
    }
    else {
        print "***ALL TESTS SUCCESSFUL***\n";
    }
}

#--------------------------------
sub Help {
    ( my $cmd ) = ( $0 =~ m#([^/]+)$# );
    print <<EOF;
$cmd: generate run CMVP algorithm tests
	--debug                     Enable debug output
	--dir=<dirname>             Optional root for *.req file search
	--filter=<regexp>
	--onedir <dirname>          Assume all components in current directory
	--rspdir=<dirname>          Name of subdirectories containing *.rsp files, default "rsp"
	--shwrap_prefix=<prefix>
	--tprefix=<prefix>
	--ignore-bogus              Ignore duplicate or bogus files
	--ignore-missing            Ignore missing test files
	--quiet                     Shhh....
	--generate                  Generate algorithm test output
	--win32                     Win32 environment
	--enable-<alg>		    Enable algorithm set <alg>.
	--disable-<alg>		    Disable algorithm set <alg>.
	Where <alg> can be one of:
EOF

while (my ($key, $value) = each %fips_enabled)
	{
	printf "\t\t%-20s(%s by default)\n", $key ,
			$value ? "enabled" : "disabled";
	}
}

# Sanity check to see if all necessary executables exist

sub sanity_check_exe {
    my ( $win32, $tprefix, $shwrap_prefix ) = @_;
    my %exe_list;
    my $bad = 0;
    $exe_list{ $shwrap_prefix . "shlib_wrap.sh" } = 1 unless $win32;
    foreach (@fips_test_list) {
        next unless ref($_);
        my $cmd = $_->[1];
        $cmd =~ s/ .*$//;
        $cmd = $tprefix . $cmd;
        $cmd .= ".exe" if $win32;
        $exe_list{$cmd} = 1;
    }

    foreach ( sort keys %exe_list ) {
        if ( !-f $_ ) {
            print STDERR "ERROR: can't find executable $_\n";
            $bad = 1;
        }
    }
    if ($bad) {
        print STDERR "FATAL ERROR: executables missing\n";
        exit(1);
    }
    elsif ($debug) {
        print STDERR "Executable sanity check passed OK\n";
    }
}

# Search for all request and response files

sub find_files {
    my ( $filter, $dir ) = @_;
    my ( $dirh, $testname );
    opendir( $dirh, $dir );
    while ( $_ = readdir($dirh) ) {
        next if ( $_ eq "." || $_ eq ".." );
        $_ = "$dir/$_";
        if ( -f "$_" ) {
            if (/\/([^\/]*)\.rsp$/) {
                $testname = fix_pss( $1, $_ );
                if ( exists $fips_tests{$testname} ) {
                    if ( $fips_tests{$testname}->[3] eq "" ) {
                        $fips_tests{$testname}->[3] = $_;
                    }
                    else {
                        print STDERR
"WARNING: duplicate response file $_ for test $testname\n";
                        $nbogus++;
                    }
                }
                else {
                    print STDERR "WARNING: bogus file $_\n";
                    $nbogus++;
                }
            }
            next unless /$filter.*\.req$/i;
            if (/\/([^\/]*)\.req$/) {
                $testname = fix_pss( $1, $_ );
                if ( exists $fips_tests{$testname} ) {
                    if ( $fips_tests{$testname}->[2] eq "" ) {
                        $fips_tests{$testname}->[2] = $_;
                    }
                    else {
                        print STDERR
"WARNING: duplicate request file $_ for test $testname\n";
                        $nbogus++;
                    }

                }
                elsif ( !/SHAmix\.req$/ ) {
                    print STDERR "WARNING: unrecognized filename $_\n";
                    $nbogus++;
                }
            }
        }
        elsif ( -d "$_" ) {
            find_files( $filter, $_ );
        }
    }
    closedir($dirh);
}

sub fix_pss {
    my ( $test, $path ) = @_;
    my $sl = "";
    local $_;
    if ( $test =~ /PSS/ ) {
        open( IN, $path ) || die "Can't Open File $path";
        while (<IN>) {
            if (/^\s*#\s*salt\s+len:\s+(\d+)\s*$/i) {
                $sl = $1;
                last;
            }
        }
        close IN;
        if ( $sl eq "" ) {
            print STDERR "WARNING: No Salt length detected for file $path\n";
        }
        else {
            return $test . "($sl)";
        }
    }
    return $test;
}

sub sanity_check_files {
    my $bad = 0;
    foreach (@fips_test_list) {
        next unless ref($_);
        my ( $tst, $cmd, $req, $resp ) = @$_;

        #print STDERR "FILES $tst, $cmd, $req, $resp\n";
        if ( $req eq "" ) {
            print STDERR "WARNING: missing request file for $tst\n";
            $bad = 1;
            next;
        }
        if ( $verify && $resp eq "" ) {
            print STDERR "WARNING: no response file for test $tst\n";
            $bad = 1;
        }
        elsif ( !$verify && $resp ne "" ) {
            print STDERR "WARNING: response file $resp will be overwritten\n";
        }
    }
    if ($bad) {
        print STDERR "ERROR: test vector file set not complete\n";
        exit(1) unless $ignore_missing;
    }
    if ($nbogus) {
        print STDERR
          "ERROR: $nbogus bogus or duplicate request and response files\n";
        exit(1) unless $ignore_bogus;
    }
    if ( $debug && !$nbogus && !$bad ) {
        print STDERR "test vector file set complete\n";
    }
}

sub run_tests {
    my ( $verify, $win32, $tprefix, $filter, $tvdir ) = @_;
    my ( $tname, $tref );
    my $bad = 0;
    foreach (@fips_test_list) {
        if ( !ref($_) ) {
            print "Running $_ tests\n" unless $quiet;
            next;
        }
        my ( $tname, $tcmd, $req, $rsp ) = @$_;
        my $out = $rsp;
        if ($verify) {
            $out =~ s/\.rsp$/.tst/;
        }
        if ( $req eq "" ) {
            print STDERR
              "WARNING: Request file for $tname missing: test skipped\n";
            $skipcnt++;
            next;
        }
        if ( $verify && $rsp eq "" ) {
            print STDERR
              "WARNING: Response file for $tname missing: test skipped\n";
            $skipcnt++;
            next;
        }
        elsif ( !$verify ) {
            if ( $rsp ne "" ) {
                print STDERR "WARNING: Response file for $tname deleted\n";
                unlink $rsp;
            }
            $out = $req;
            $out =~ s|/req/(\S+)\.req|/$rspdir/$1.rsp|;
            my $outdir = $out;
            $outdir =~ s|/[^/]*$||;
            if ( !-d $outdir ) {
                print STDERR "DEBUG: Creating directory $outdir\n" if $debug;
                mkdir($outdir) || die "Can't create directory $outdir";
            }
        }
        my $cmd = "$cmd_prefix$tprefix$tcmd ";
        if ( $tcmd =~ /-f$/ ) {
            $cmd .= "\"$req\" \"$out\"";
        }
        else {
            $cmd .= "<\"$req\" >\"$out\"";
        }
        print STDERR "DEBUG: running test $tname\n" if ( $debug && !$verify );
        system($cmd);
        if ( $? != 0 ) {
            print STDERR
              "WARNING: error executing test $tname for command: $cmd\n";
            $runerr++;
            next;
        }
        if ($verify) {
            if ( exists $verify_special{$tname} ) {
                my $vout = $rsp;
                $vout =~ s/\.rsp$/.ver/;
                $tcmd = $verify_special{$tname};
                $cmd  = "$cmd_prefix$tprefix$tcmd ";
                $cmd .= "<\"$out\" >\"$vout\"";
                system($cmd);
                if ( $? != 0 ) {
                    print STDERR
                      "WARNING: error executing verify test $tname $cmd\n";
                    $scheckrunerr++;
                    next;
                }
                my ( $fcount, $pcount ) = ( 0, 0 );
                open VER, "$vout";
                while (<VER>) {
                    if (/^Result\s*=\s*(\S*)\s*$/i)

                    {
                        if ( $1 eq "F" ) {
                            $fcount++;
                        }
                        else {
                            $pcount++;
                        }
                    }
                }
                close VER;

                unlink $vout;
                if ( $fcount || $debug ) {
                    print STDERR "DEBUG: $tname, Pass=$pcount, Fail=$fcount\n";
                }
                if ( $fcount || !$pcount ) {
                    $scheckerr++;
                }
                else {
                    $scheckok++;
                }

            }
            elsif ( !cmp_file( $tname, $rsp, $out ) ) {
                $cmperr++;
            }
            else {
                $cmpok++;
            }
            unlink $out;
        }
    }
}

sub cmp_file {
    my ( $tname, $rsp, $tst ) = @_;
    my ( $rspf,    $tstf );
    my ( $rspline, $tstline );
    if ( !open( $rspf, $rsp ) ) {
        print STDERR "ERROR: can't open request file $rsp\n";
        return 0;
    }
    if ( !open( $tstf, $tst ) ) {
        print STDERR "ERROR: can't open output file $tst\n";
        return 0;
    }
    for ( ; ; ) {
        $rspline = next_line($rspf);
        $tstline = next_line($tstf);
        if ( !defined($rspline) && !defined($tstline) ) {
            print STDERR "DEBUG: $tname file comparison OK\n" if $debug;
            return 1;
        }
        if ( !defined($rspline) ) {
            print STDERR "ERROR: $tname EOF on $rsp\n";
            return 0;
        }
        if ( !defined($tstline) ) {
            print STDERR "ERROR: $tname EOF on $tst\n";
            return 0;
        }

        # Workaround for bug in RAND des2 test output */
        if ( $tstline =~ /^Key2 =/ && $rspline =~ /^Key1 =/ ) {
            $rspline =~ s/^Key1/Key2/;
        }

        if ( $tstline ne $rspline ) {
            print STDERR "ERROR: $tname mismatch:\n";
            print STDERR "\t \"$tstline\" != \"$rspline\"\n";
            return 0;
        }
    }
    return 1;
}

sub next_line {
    my ($in) = @_;

    while (<$in>) {
        chomp;

        # Delete comments
        s/#.*$//;

        # Ignore blank lines
        next if (/^\s*$/);

        # Translate multiple space into one
        s/\s+/ /g;
	# Delete trailing whitespace
	s/\s+$//;
        return $_;
    }
    return undef;
}
