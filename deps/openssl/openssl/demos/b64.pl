#!/usr/local/bin/perl

#
# Make PEM encoded data have lines of 64 bytes of data
#

while (<>)
	{
	if (/^-----BEGIN/ .. /^-----END/)
		{
		if (/^-----BEGIN/) { $first=$_; next; }
		if (/^-----END/) { $last=$_; next; }
		$out.=$_;
		}
	}
$out =~ s/\s//g;
$out =~ s/(.{64})/$1\n/g;
print "$first$out\n$last\n";


