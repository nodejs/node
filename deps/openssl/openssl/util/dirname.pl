#!/usr/local/bin/perl

if ($#ARGV < 0) {
    die "dirname.pl: too few arguments\n";
} elsif ($#ARGV > 0) {
    die "dirname.pl: too many arguments\n";
}

my $d = $ARGV[0];

if ($d =~ m|.*/.*|) {
    $d =~ s|/[^/]*$||;
} else {
    $d = ".";
}

print $d,"\n";
exit(0);
