#!/usr/bin/perl -w
use strict;

# Use as $0
# Use as $0 -noname

my $did_library;
my $did_description;
my $do_exports;
my @imports;
my $noname = (@ARGV and $ARGV[0] eq '-noname' and shift);
while (<>) {
  unless ($did_library) {
    s/\b(cryptssl)\b/crypto/ and $did_library = $1 if /^LIBRARY\s+cryptssl\b/;
    s/\b(open_ssl)\b/ssl/    and $did_library = $1 if /^LIBRARY\s+open_ssl\b/;
  }
  unless ($did_description) {
    s&^(DESCRIPTION\s+(['"])).*&${1}\@#www.openssl.org/:#\@forwarder DLL for pre-0.9.7c+ OpenSSL to the new dll naming scheme$2& and $did_description++;
  }
  if ($do_exports) {{
    last unless /\S/;
    warn, last unless /^ \s* ( \w+ ) \s+ \@(\d+)\s*$/x;
    push @imports, [$1, $2];
    s/$/ NONAME/ if $noname;
  }}
  $do_exports++ if not $do_exports and /^EXPORTS/;
  print $_;
}
print "IMPORTS\n";
for my $imp (@imports) {
  print "\t$imp->[0]=$did_library.$imp->[1]\n";
}
