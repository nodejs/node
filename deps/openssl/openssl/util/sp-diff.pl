#!/usr/local/bin/perl
#
# This file takes as input, the files that have been output from
# ssleay speed.
# It prints a table of the relative differences with %100 being 'no difference'
#

($#ARGV == 1) || die "$0 speedout1 speedout2\n";

%one=&loadfile($ARGV[0]);
%two=&loadfile($ARGV[1]);

$line=0;
foreach $a ("md2","md4","md5","sha","sha1","rc4","des cfb","des cbc","des ede3",
	"idea cfb","idea cbc","rc2 cfb","rc2 cbc","blowfish cbc","cast cbc")
	{
	if (defined($one{$a,8}) && defined($two{$a,8}))
		{
		print "type              8 byte%    64 byte%   256 byte%  1024 byte%  8192 byte%\n"
			unless $line;
		$line++;
		printf "%-12s ",$a;
		foreach $b (8,64,256,1024,8192)
			{
			$r=$two{$a,$b}/$one{$a,$b}*100;
			printf "%12.2f",$r;
			}
		print "\n";
		}
	}

foreach $a	(
		"rsa  512","rsa 1024","rsa 2048","rsa 4096",
		"dsa  512","dsa 1024","dsa 2048",
		)
	{
	if (defined($one{$a,1}) && defined($two{$a,1}))
		{
		$r1=($one{$a,1}/$two{$a,1})*100;
		$r2=($one{$a,2}/$two{$a,2})*100;
		printf "$a bits %%    %6.2f %%    %6.2f\n",$r1,$r2;
		}
	}

sub loadfile
	{
	local($file)=@_;
	local($_,%ret);

	open(IN,"<$file") || die "unable to open '$file' for input\n";
	$header=1;
	while (<IN>)
		{
		$header=0 if /^[dr]sa/;
		if (/^type/) { $header=0; next; }
		next if $header;
		chop;
		@a=split;
		if ($a[0] =~ /^[dr]sa$/)
			{
			($n,$t1,$t2)=($_ =~ /^([dr]sa\s+\d+)\s+bits\s+([.\d]+)s\s+([.\d]+)/);
			$ret{$n,1}=$t1;
			$ret{$n,2}=$t2;
			}
		else
			{
			$n=join(' ',grep(/[^k]$/,@a));
			@k=grep(s/k$//,@a);
			
			$ret{$n,   8}=$k[0];
			$ret{$n,  64}=$k[1];
			$ret{$n, 256}=$k[2];
			$ret{$n,1024}=$k[3];
			$ret{$n,8192}=$k[4];
			}
		}
	close(IN);
	return(%ret);
	}

