#! /usr/bin/env perl
# Copyright 2011-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

sub check_env
	{
	my @ret;
	foreach (@_)
		{
		die "Environment variable $_ not defined!\n" unless exists $ENV{$_};
		push @ret, $ENV{$_};
		}
	return @ret;
	}


my ($fips_cc,$fips_cc_args, $fips_link,$fips_target, $fips_libdir, $sha1_exe)
	 = check_env("FIPS_CC", "FIPS_CC_ARGS", "FIPS_LINK", "FIPS_TARGET",
	 	"FIPSLIB_D", "FIPS_SHA1_EXE");



if (exists $ENV{"PREMAIN_DSO_EXE"})
	{
	$fips_premain_dso = $ENV{"PREMAIN_DSO_EXE"};
	}
	else
	{
	$fips_premain_dso = "";
	}

check_hash($sha1_exe, "fips_premain.c");
check_hash($sha1_exe, "fipscanister.lib");


print "Integrity check OK\n";

if (is_premain_linked(@ARGV)) {
	print "$fips_cc $fips_cc_args $fips_libdir/fips_premain.c\n";
	system "$fips_cc $fips_cc_args $fips_libdir/fips_premain.c";
	die "First stage Compile failure" if $? != 0;
} elsif (!defined($ENV{FIPS_SIG})) {
	die "no fips_premain.obj linked";
}

print "$fips_link @ARGV\n";
system "$fips_link @ARGV";
die "First stage Link failure" if $? != 0;

if (defined($ENV{FIPS_SIG})) {
	print "$ENV{FIPS_SIG} $fips_target\n";
	system "$ENV{FIPS_SIG} $fips_target";
	die "$ENV{FIPS_SIG} $fips_target failed" if $? != 0;
	exit;
}

print "$fips_premain_dso $fips_target\n";
system("$fips_premain_dso $fips_target >$fips_target.sha1");
die "Get hash failure" if $? != 0;
open my $sha1_res, '<', $fips_target.".sha1" or die "Get hash failure";
$fips_hash=<$sha1_res>;
close $sha1_res;
unlink $fips_target.".sha1";
$fips_hash =~ s|\R$||;          # Better chomp
die "Get hash failure" if $? != 0;


print "$fips_cc -DHMAC_SHA1_SIG=\\\"$fips_hash\\\" $fips_cc_args $fips_libdir/fips_premain.c\n";
system "$fips_cc -DHMAC_SHA1_SIG=\\\"$fips_hash\\\" $fips_cc_args $fips_libdir/fips_premain.c";
die "Second stage Compile failure" if $? != 0;


print "$fips_link @ARGV\n";
system "$fips_link @ARGV";
die "Second stage Link failure" if $? != 0;

sub is_premain_linked
	{
	return 1 if (grep /fips_premain\.obj/,@_);
	foreach (@_)
		{
		if (/^@(.*)/ && -f $1)
			{
			open FD,$1 or die "can't open $1";
			my $ret = (grep /fips_premain\.obj/,<FD>)?1:0;
			close FD;
			return $ret;
			}
		}
	return 0;
	}

sub check_hash
	{
	my ($sha1_exe, $filename) = @_;
	my ($hashfile, $hashval);

	open(IN, "${fips_libdir}/${filename}.sha1") || die "Cannot open file hash file ${fips_libdir}/${filename}.sha1";
	$hashfile = <IN>;
	close IN;
	$hashval = `$sha1_exe ${fips_libdir}/$filename`;
	$hashfile =~ s|\R$||;    # Better chomp
	$hashval =~ s|\R$||;     # Better chomp
	$hashfile =~ s/^.*=\s+//;
	$hashval =~ s/^.*=\s+//;
	die "Invalid hash syntax in file" if (length($hashfile) != 40);
	die "Invalid hash received for file" if (length($hashval) != 40);
	die "***HASH VALUE MISMATCH FOR FILE $filename ***" if ($hashval ne $hashfile); 
	}


