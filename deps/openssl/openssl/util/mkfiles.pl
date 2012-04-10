#!/usr/local/bin/perl
#
# This is a hacked version of files.pl for systems that can't do a 'make files'.
# Do a perl util/mkminfo.pl >MINFO to build MINFO
# Written by Steve Henson 1999.

# List of directories to process

my @dirs = (
".",
"crypto",
"crypto/md2",
"crypto/md4",
"crypto/md5",
"crypto/sha",
"crypto/mdc2",
"crypto/hmac",
"crypto/ripemd",
"crypto/des",
"crypto/rc2",
"crypto/rc4",
"crypto/rc5",
"crypto/idea",
"crypto/bf",
"crypto/cast",
"crypto/aes",
"crypto/camellia",
"crypto/seed",
"crypto/modes",
"crypto/bn",
"crypto/rsa",
"crypto/dsa",
"crypto/dso",
"crypto/dh",
"crypto/ec",
"crypto/ecdh",
"crypto/ecdsa",
"crypto/buffer",
"crypto/bio",
"crypto/stack",
"crypto/lhash",
"crypto/rand",
"crypto/err",
"crypto/objects",
"crypto/evp",
"crypto/asn1",
"crypto/pem",
"crypto/x509",
"crypto/x509v3",
"crypto/cms",
"crypto/conf",
"crypto/jpake",
"crypto/txt_db",
"crypto/pkcs7",
"crypto/pkcs12",
"crypto/comp",
"crypto/engine",
"crypto/ocsp",
"crypto/ui",
"crypto/krb5",
#"crypto/store",
"crypto/pqueue",
"crypto/whrlpool",
"crypto/ts",
"ssl",
"apps",
"engines",
"engines/ccgost",
"test",
"tools"
);

%top;

foreach (@dirs) {
	&files_dir ($_, "Makefile");
}

exit(0);

sub files_dir
{
my ($dir, $makefile) = @_;

my %sym;

open (IN, "$dir/$makefile") || die "Can't open $dir/$makefile";

my $s="";

while (<IN>)
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
				$b=<IN>;
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

		$o =~ s/\$[({]([^)}]+)[)}]/$top{$1} or $sym{$1}/ge;
		$sym{$s}=($top{$s} or $o);
		}
	}

print "RELATIVE_DIRECTORY=$dir\n";

foreach (sort keys %sym)
	{
	print "$_=$sym{$_}\n";
	}
if ($dir eq "." && defined($sym{"BUILDENV"}))
	{
	foreach (split(' ',$sym{"BUILDENV"}))
		{
		/^(.+)=/;
		$top{$1}=$sym{$1};
		}
	}

print "RELATIVE_DIRECTORY=\n";

close (IN);
}
