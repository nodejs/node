#! /usr/bin/env perl
use 5.10.0;
use strict;
my $arch = $ENV{ARC};
my $ret;
if ($arch =~ /^darwin/) {
  $ret = "Apple LLVM version 10.0.0 (clang-1000.11.45.2)\n";
} else {
  $ret = `gcc -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1`;
}
print STDOUT $ret;
