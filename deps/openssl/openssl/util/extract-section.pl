#!/usr/bin/perl

while(<STDIN>) {
	if (/=for\s+comment\s+openssl_manual_section:(\S+)/)
		{
		print "$1\n";
		exit 0;
		}
}

print "$ARGV[0]\n";

