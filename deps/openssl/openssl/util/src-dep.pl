#!/usr/local/bin/perl

# we make up an array of
# $file{function_name}=filename;
# $unres{filename}="func1 func2 ...."
$debug=1;
#$nm_func="parse_linux";
$nm_func="parse_solaris";

foreach (@ARGV)
	{
	&$nm_func($_);
	}

foreach $file (sort keys %unres)
	{
	@a=split(/\s+/,$unres{$file});
	%ff=();
	foreach $func (@a)
		{
		$f=$file{$func};
		$ff{$f}=1 if $f ne "";
		}

	foreach $a (keys %ff)
		{ $we_need{$file}.="$a "; }
	}

foreach $file (sort keys %we_need)
	{
#	print "	$file $we_need{$file}\n";
	foreach $bit (split(/\s+/,$we_need{$file}))
		{ push(@final,&walk($bit)); }

	foreach (@final) { $fin{$_}=1; }
	@final="";
	foreach (sort keys %fin)
		{ push(@final,$_); }

	print "$file: @final\n";
	}

sub walk
	{
	local($f)=@_;
	local(@a,%seen,@ret,$r);

	@ret="";
	$f =~ s/^\s+//;
	$f =~ s/\s+$//;
	return "" if ($f =~ "^\s*$");

	return(split(/\s/,$done{$f})) if defined ($done{$f});

	return if $in{$f} > 0;
	$in{$f}++;
	push(@ret,$f);
	foreach $r (split(/\s+/,$we_need{$f}))
		{
		push(@ret,&walk($r));
		}
	$in{$f}--;
	$done{$f}=join(" ",@ret);
	return(@ret);
	}

sub parse_linux
	{
	local($name)=@_;

	open(IN,"nm $name|") || die "unable to run 'nn $name':$!\n";
	while (<IN>)
		{
		chop;
		next if /^\s*$/;
		if (/^[^[](.*):$/)
			{
			$file=$1;
			$file="$1.c" if /\[(.*).o\]/;
			print STDERR "$file\n";
			$we_need{$file}=" ";
			next;
			}

		@a=split(/\s*\|\s*/);
		next unless $#a == 7;
		next unless $a[4] eq "GLOB";
		if ($a[6] eq "UNDEF")
			{
			$unres{$file}.=$a[7]." ";
			}
		else
			{
			if ($file{$a[7]} ne "")
				{
				print STDERR "duplicate definition of $a[7],\n$file{$a[7]} and $file \n";
				}
			else
				{
				$file{$a[7]}=$file;
				}
			}
		}
	close(IN);
	}

sub parse_solaris
	{
	local($name)=@_;

	open(IN,"nm $name|") || die "unable to run 'nn $name':$!\n";
	while (<IN>)
		{
		chop;
		next if /^\s*$/;
		if (/^(\S+):$/)
			{
			$file=$1;
			#$file="$1.c" if $file =~ /^(.*).o$/;
			print STDERR "$file\n";
			$we_need{$file}=" ";
			next;
			}
		@a=split(/\s*\|\s*/);
		next unless $#a == 7;
		next unless $a[4] eq "GLOB";
		if ($a[6] eq "UNDEF")
			{
			$unres{$file}.=$a[7]." ";
			print STDERR "$file needs $a[7]\n" if $debug;
			}
		else
			{
			if ($file{$a[7]} ne "")
				{
				print STDERR "duplicate definition of $a[7],\n$file{$a[7]} and $file \n";
				}
			else
				{
				$file{$a[7]}=$file;
				print STDERR "$file has $a[7]\n" if $debug;
				}
			}
		}
	close(IN);
	}

