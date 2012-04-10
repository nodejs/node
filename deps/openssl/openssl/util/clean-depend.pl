#!/usr/local/bin/perl -w
# Clean the dependency list in a makefile of standard includes...
# Written by Ben Laurie <ben@algroup.co.uk> 19 Jan 1999

use strict;

while(<STDIN>) {
    print;
    last if /^# DO NOT DELETE THIS LINE/;
}

my %files;

my $thisfile="";
while(<STDIN>) {
    my ($dummy, $file,$deps)=/^((.*):)? (.*)$/;
    my $origfile="";
    $thisfile=$file if defined $file;
    next if !defined $deps;
    $origfile=$thisfile;
    $origfile=~s/\.o$/.c/;
    my @deps=split ' ',$deps;
    @deps=grep(!/^\//,@deps);
    @deps=grep(!/^\\$/,@deps);
    @deps=grep(!/^$origfile$/,@deps);
# pull out the kludged kerberos header (if present).
    @deps=grep(!/^[.\/]+\/krb5.h/,@deps);
    push @{$files{$thisfile}},@deps;
}

my $file;
foreach $file (sort keys %files) {
    my $len=0;
    my $dep;
    my $origfile=$file;
    $origfile=~s/\.o$/.c/;
    $file=~s/^\.\///;
    push @{$files{$file}},$origfile;
    my $prevdep="";

    # Remove leading ./ before sorting
    my @deps = map { $_ =~ s/^\.\///; $_ } @{$files{$file}};

    foreach $dep (sort @deps) {
	$dep=~s/^\.\///;
	next if $prevdep eq $dep; # to exterminate duplicates...
	$prevdep = $dep;
	$len=0 if $len+length($dep)+1 >= 80;
	if($len == 0) {
	    print "\n$file:";
	    $len=length($file)+1;
	}
	print " $dep";
	$len+=length($dep)+1;
    }
}

print "\n";
