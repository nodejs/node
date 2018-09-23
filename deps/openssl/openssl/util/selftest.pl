#!/usr/local/bin/perl -w
#
# Run the test suite and generate a report
#

if (! -f "Configure") {
    print "Please run perl util/selftest.pl in the OpenSSL directory.\n";
    exit 1;
}

my $report="testlog";
my $os="??";
my $version="??";
my $platform0="??";
my $platform="??";
my $options="??";
my $last="??";
my $ok=0;
my $cc="cc";
my $cversion="??";
my $sep="-----------------------------------------------------------------------------\n";
my $not_our_fault="\nPlease ask your system administrator/vendor for more information.\n[Problems with your operating system setup should not be reported\nto the OpenSSL project.]\n";

open(OUT,">$report") or die;

print OUT "OpenSSL self-test report:\n\n";

$uname=`uname -a`;
$uname="??\n" if $uname eq "";

$c=`sh config -t`;
foreach $_ (split("\n",$c)) {
    $os=$1 if (/Operating system: (.*)$/);
    $platform0=$1 if (/Configuring for (.*)$/);
}

system "sh config" if (! -f "Makefile");

if (open(IN,"<Makefile")) {
    while (<IN>) {
	$version=$1 if (/^VERSION=(.*)$/);
	$platform=$1 if (/^PLATFORM=(.*)$/);
	$options=$1 if (/^OPTIONS=(.*)$/);
	$cc=$1 if (/^CC= *(.*)$/);
    }
    close(IN);
} else {
    print OUT "Error running config!\n";
}

$cversion=`$cc -v 2>&1`;
$cversion=`$cc -V 2>&1` if $cversion =~ "[Uu]sage";
$cversion=`$cc -V |head -1` if $cversion =~ "Error";
$cversion=`$cc --version` if $cversion eq "";
$cversion =~ s/Reading specs.*\n//;
$cversion =~ s/usage.*\n//;
chomp $cversion;

if (open(IN,"<CHANGES")) {
    while(<IN>) {
	if (/\*\) (.{0,55})/ && !/applies to/) {
	    $last=$1;
	    last;
	}
    }
    close(IN);
}

print OUT "OpenSSL version:  $version\n";
print OUT "Last change:      $last...\n";
print OUT "Options:          $options\n" if $options ne "";
print OUT "OS (uname):       $uname";
print OUT "OS (config):      $os\n";
print OUT "Target (default): $platform0\n";
print OUT "Target:           $platform\n";
print OUT "Compiler:         $cversion\n";
print OUT "\n";

print "Checking compiler...\n";
if (open(TEST,">cctest.c")) {
    print TEST "#include <stdio.h>\n#include <stdlib.h>\n#include <errno.h>\nmain(){printf(\"Hello world\\n\");}\n";
    close(TEST);
    system("$cc -o cctest cctest.c");
    if (`./cctest` !~ /Hello world/) {
	print OUT "Compiler doesn't work.\n";
	print OUT $not_our_fault;
	goto err;
    }
    system("ar r cctest.a /dev/null");
    if (not -f "cctest.a") {
	print OUT "Check your archive tool (ar).\n";
	print OUT $not_our_fault;
	goto err;
    }
} else {
    print OUT "Can't create cctest.c\n";
}
if (open(TEST,">cctest.c")) {
    print TEST "#include <stdio.h>\n#include <stdlib.h>\n#include <openssl/opensslv.h>\nmain(){printf(OPENSSL_VERSION_TEXT);}\n";
    close(TEST);
    system("$cc -o cctest -Iinclude cctest.c");
    $cctest = `./cctest`;
    if ($cctest !~ /OpenSSL $version/) {
	if ($cctest =~ /OpenSSL/) {
	    print OUT "#include uses headers from different OpenSSL version!\n";
	} else {
	    print OUT "Can't compile test program!\n";
	}
	print OUT $not_our_fault;
	goto err;
    }
} else {
    print OUT "Can't create cctest.c\n";
}

print "Running make...\n";
if (system("make 2>&1 | tee make.log") > 255) {

    print OUT "make failed!\n";
    if (open(IN,"<make.log")) {
	print OUT $sep;
	while (<IN>) {
	    print OUT;
	}
	close(IN);
	print OUT $sep;
    } else {
	print OUT "make.log not found!\n";
    }
    goto err;
}

# Not sure why this is here.  The tests themselves can detect if their
# particular feature isn't included, and should therefore skip themselves.
# To skip *all* tests just because one algorithm isn't included is like
# shooting mosquito with an elephant gun...
#                   -- Richard Levitte, inspired by problem report 1089
#
#$_=$options;
#s/no-asm//;
#s/no-shared//;
#s/no-krb5//;
#if (/no-/)
#{
#    print OUT "Test skipped.\n";
#    goto err;
#}

print "Running make test...\n";
if (system("make test 2>&1 | tee maketest.log") > 255)
 {
    print OUT "make test failed!\n";
} else {
    $ok=1;
}

if ($ok and open(IN,"<maketest.log")) {
    while (<IN>) {
	$ok=2 if /^platform: $platform/;
    }
    close(IN);
}

if ($ok != 2) {
    print OUT "Failure!\n";
    if (open(IN,"<make.log")) {
	print OUT $sep;
	while (<IN>) {
	    print OUT;
	}
	close(IN);
	print OUT $sep;
    } else {
	print OUT "make.log not found!\n";
    }
    if (open(IN,"<maketest.log")) {
	while (<IN>) {
	    print OUT;
	}
	close(IN);
	print OUT $sep;
    } else {
	print OUT "maketest.log not found!\n";
    }
} else {
    print OUT "Test passed.\n";
}
err:
close(OUT);

print "\n";
open(IN,"<$report") or die;
while (<IN>) {
    if (/$sep/) {
	print "[...]\n";
	last;
    }
    print;
}
print "\nTest report in file $report\n";

