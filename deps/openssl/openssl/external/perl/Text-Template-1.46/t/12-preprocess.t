#!perl
#
# Tests for PREPROCESSOR features
# These tests first appeared in version 1.25.

use Text::Template::Preprocess;

die "This is the test program for Text::Template::Preprocess version 1.46.
You are using version $Text::Template::Preprocess::VERSION instead.
That does not make sense.\n
Aborting"
  unless $Text::Template::Preprocess::VERSION == 1.46;

$TMPFILE = "tt$$";

print "1..8\n";
my $n = 1;

my $py = sub { tr/x/y/ };
my $pz = sub { tr/x/z/ };

my $t = 'xxx The value of $x is {$x}';
my $outx = 'xxx The value of $x is 119';
my $outy = 'yyy The value of $y is 23';
my $outz = 'zzz The value of $z is 5';
open TF, "> $TMPFILE" or die "Couldn't open test file: $!; aborting";
print TF $t;
close TF;

@result = ($outx, $outy, $outz, $outz);
for my $trial (1, 0) {
  for my $test (0 .. 3) {
    my $tmpl;
    if ($trial == 0) {
      $tmpl = new Text::Template::Preprocess 
	(TYPE => 'STRING', SOURCE => $t) or die;
    } else {
      open TF, "< $TMPFILE" or die "Couldn't open test file: $!; aborting";
      $tmpl = new Text::Template::Preprocess 
	(TYPE => 'FILEHANDLE', SOURCE => \*TF) or die;
    }
    $tmpl->preprocessor($py) if ($test & 1) == 1;
    my @args = ((($test & 2) == 2) ? (PREPROCESSOR => $pz) : ());
    my $o = $tmpl->fill_in(@args, 
			   HASH => {x => 119, 'y' => 23, z => 5});
#    print STDERR "$o/$result[$test]\n";
    print +(($o eq $result[$test]) ? '' : 'not '), "ok $n\n";
    $n++;
  }
}

unlink $TMPFILE;
