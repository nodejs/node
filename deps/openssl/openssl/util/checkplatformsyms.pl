#! /usr/bin/env perl
# Copyright 2006-2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use warnings;
use strict;
use Config;

my $expectedsyms=$ARGV[0];

shift(@ARGV);

# Check that object files exist
foreach (@ARGV) {
    unless (-f $_ && -r $_) {
        die "Path is not a regular readable file: '$_'";
    }
}

my $objlist;
my $objfilelist = join(" ", @ARGV);
my $expsyms;
my $exps;
my $OBJFH;
my $cmd;

if ($Config{osname} eq "MSWin32") {
        my $currentdll = "";
        $cmd = "dumpbin /imports " . $objfilelist;
        my @symlist;
        open $expsyms, '<', $expectedsyms or die;
        {
            local $/;
            $exps=<$expsyms>;
        }
        close($expsyms);
        open($OBJFH, "$cmd|") or die "Cannot open process: $!";
        while (<$OBJFH>)
        {
            chomp;
            my $dllfile = $_;
            $dllfile =~ s/( +)(.*)(\.dll)(.*)/DLLFILE $2/;
            if (index($dllfile, "DLLFILE") >= 0) {
                $currentdll = substr($dllfile, 8);
                $currentdll =~ s/^\s+|s+$//g;
            }
            # filter imports from our own library
            if ("$currentdll" !~ /^libcrypto-[1-9][0-9]*(-x64)?$/) {
                my $line = $_;
                $line =~ s/                          [0-9a-fA-F]{1,2} /SYMBOL /;
                if (index($line, "SYMBOL") != -1) {
                    $line =~ s/.*SYMBOL //;
                    push(@symlist, $line);
                }
            }
        }

        close($OBJFH);
        ($? >> 8 == 0) or die "Command '$cmd' has failed.";

        my $ok = 1;
        foreach (@symlist) {
            chomp;
            if (index($exps, $_) < 0) {
                print "Symbol $_ not in the allowed platform symbols list\n";
                $ok = 0;
            }
        }
        exit !$ok;
    }
else {
        $cmd = "objdump -t " . $objfilelist . " | awk " .
            "'/\\\\*UND\\\\*/ {" .
                "split(\$NF, sym_lib, \"@\");" .
                "if (sym_lib[2] !~ \"OPENSSL_[1-9][0-9]*\\\\.[0-9]+\\\\.[0-9]+\$\")" .
                    "syms[sym_lib[1]] = 1;" .
            "}" .
            "END { for (s in syms) print s; };'";

        open $expsyms, '<', $expectedsyms or die;
        {
            local $/;
            $exps=<$expsyms>;
        }
        close($expsyms);

        open($OBJFH, "$cmd|") or die "Cannot open process: $!";
        my $ok = 1;
        while (<$OBJFH>)
        {
                chomp;
                if (index($exps, $_) < 0) {
                    print "Symbol $_ not in the allowed platform symbols list\n";
                    $ok = 0;
                }
        }
        close($OBJFH);

        exit !(!($? >> 8) || !$ok);
    }
