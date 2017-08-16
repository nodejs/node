#!/usr/local/bin/perl
#
# used to generate the file MINFO for use by util/mk1mf.pl
# It is basically a list of all variables from the passed makefile
#

while ($ARGV[0] =~ /^(\S+)\s*=(.*)$/)
	{
	$sym{$1} = $2;
	shift;
	}

$s="";
while (<>)
	{
	chop;
	s/#.*//;
	if (/^(\S+)\s*=\s*(.*)$/)
		{
		$o="";
		($s,$b)=($1,$2);
		for (;;)
			{
			if ($b =~ /\\$/)
				{
				chop($b);
				$o.=$b." ";
				$b=<>;
				chop($b);
				}
			else
				{
				$o.=$b." ";
				last;
				}
			}
		$o =~ s/^\s+//;
		$o =~ s/\s+$//;
		$o =~ s/\s+/ /g;

		$o =~ s/\$[({]([^)}]+)[)}]/$sym{$1}/g;
		$sym{$s}=$o if !exists $sym{$s};
		}
	}

$pwd=`pwd`; chop($pwd);

if ($sym{'TOP'} eq ".")
	{
	$n=0;
	$dir=".";
	}
else	{
	$n=split(/\//,$sym{'TOP'});
	@_=split(/\//,$pwd);
	$z=$#_-$n+1;
	foreach $i ($z .. $#_) { $dir.=$_[$i]."/"; }
	chop($dir);
	}

print "RELATIVE_DIRECTORY=$dir\n";

foreach (sort keys %sym)
	{
	print "$_=$sym{$_}\n";
	}
print "RELATIVE_DIRECTORY=\n";
