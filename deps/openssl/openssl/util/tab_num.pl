#!/usr/local/bin/perl

$num=1;
$width=40;

while (<>)
	{
	chop;

	$i=length($_);

	$n=$width-$i;
	$i=int(($n+7)/8);
	print $_.("\t" x $i).$num."\n";
	$num++;
	}

