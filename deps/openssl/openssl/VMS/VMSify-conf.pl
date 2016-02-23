#! /usr/bin/perl

use strict;
use warnings;

my @directory_vars = ( "dir", "certs", "crl_dir", "new_certs_dir" );
my @file_vars = ( "database", "certificate", "serial", "crlnumber",
		  "crl", "private_key", "RANDFILE" );
while(<STDIN>) {
    chomp;
    foreach my $d (@directory_vars) {
	if (/^(\s*\#?\s*${d}\s*=\s*)\.\/([^\s\#]*)([\s\#].*)$/) {
	    $_ = "$1sys\\\$disk:\[.$2$3";
	} elsif (/^(\s*\#?\s*${d}\s*=\s*)(\w[^\s\#]*)([\s\#].*)$/) {
	    $_ = "$1sys\\\$disk:\[.$2$3";
	}
	s/^(\s*\#?\s*${d}\s*=\s*\$\w+)\/([^\s\#]*)([\s\#].*)$/$1.$2\]$3/;
	while(/^(\s*\#?\s*${d}\s*=\s*(\$\w+\.|sys\\\$disk:\[\.)[\w\.]+)\/([^\]]*)\](.*)$/) {
	    $_ = "$1.$3]$4";
	}
    }
    foreach my $f (@file_vars) {
	s/^(\s*\#?\s*${f}\s*=\s*)\.\/(.*)$/$1sys\\\$disk:\[\/$2/;
	while(/^(\s*\#?\s*${f}\s*=\s*(\$\w+|sys\\\$disk:\[)[^\/]*)\/(\w+\/[^\s\#]*)([\s\#].*)$/) {
	    $_ = "$1.$3$4";
	}
	if (/^(\s*\#?\s*${f}\s*=\s*(\$\w+|sys\\\$disk:\[)[^\/]*)\/(\w+)([\s\#].*)$/) {
	    $_ = "$1]$3.$4";
	} elsif  (/^(\s*\#?\s*${f}\s*=\s*(\$\w+|sys\\\$disk:\[)[^\/]*)\/([^\s\#]*)([\s\#].*)$/) {
	    $_ = "$1]$3$4";
	}
   }
    print $_,"\n";
}
