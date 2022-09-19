#! /usr/bin/env perl
use 5.10.0;
use strict;
my $args = join(' ', @ARGV);

my $ret;
if ($args eq '-Wa,-v -c -o /dev/null -x assembler /dev/null') {
  $ret = "GNU assembler version 2.23.52.0.1 (x86_64-redhat-linux) using BFD version version 2.23.52.0.1-30.el7_1.2 20130226\n";
} else {
  $ret = `gcc $args`;
}
print STDOUT $ret;
