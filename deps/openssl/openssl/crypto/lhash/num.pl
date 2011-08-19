#!/usr/local/bin/perl

#node     10 ->   4

while (<>)
	{
	next unless /^node/;
	chop;
	@a=split;
	$num{$a[3]}++;
	}

@a=sort {$a <=> $b } keys %num;
foreach (0 .. $a[$#a])
	{
	printf "%4d:%4d\n",$_,$num{$_};
	}
