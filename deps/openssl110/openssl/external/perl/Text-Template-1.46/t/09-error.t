#!perl
#
# test apparatus for Text::Template module
# still incomplete.

use Text::Template;

die "This is the test program for Text::Template version 1.46.
You are using version $Text::Template::VERSION instead.
That does not make sense.\n
Aborting"
  unless $Text::Template::VERSION == 1.46;

print "1..5\n";
$n = 1;

# (1-2) Missing source
eval {
  Text::Template->new();
};
unless ($@ =~ /^\QUsage: Text::Template::new(TYPE => ..., SOURCE => ...)/) {
  print STDERR $@;
  print "not ";
}
print "ok $n\n";
$n++;

eval {
  Text::Template->new(TYPE => 'FILE');
};
if ($@ =~ /^\QUsage: Text::Template::new(TYPE => ..., SOURCE => ...)/) {
  print "ok $n\n";
} else {
  print STDERR $@;
  print "not ok $n\n";
}
$n++;

# (3) Invalid type
eval {
  Text::Template->new(TYPE => 'wlunch', SOURCE => 'fish food');
};
if ($@ =~ /^\QIllegal value `WLUNCH' for TYPE parameter/) {
  print "ok $n\n";
} else {
  print STDERR $@;
  print "not ok $n\n";
}
$n++;

# (4-5) File does not exist
my $o = Text::Template->new(TYPE => 'file', 
                            SOURCE => 'this file does not exist');
print $o ? "not ok $n\n" : "ok $n\n";
$n++;
print defined($Text::Template::ERROR) 
      && $Text::Template::ERROR =~ /^Couldn't open file/
  ? "ok $n\n" : "not ok $n\n";
$n++;


exit;

