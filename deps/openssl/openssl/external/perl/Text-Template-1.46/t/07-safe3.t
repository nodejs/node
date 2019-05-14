#!perl
#
# test apparatus for Text::Template module

use Text::Template;

BEGIN {
  eval "use Safe";
  if ($@) {
    print "1..0\n";
    exit 0;
  }
}

die "This is the test program for Text::Template version 1.46.
You are using version $Text::Template::VERSION instead.
That does not make sense.\n
Aborting"
  unless $Text::Template::VERSION == 1.46;

print "1..3\n";

$n=1;

# Test the OUT feature with safe compartments

$template = q{
This line should have a 3: {1+2}

This line should have several numbers:
{ $t = ''; foreach $n (1 .. 20) { $t .= $n . ' ' } $t }
};

$templateOUT = q{
This line should have a 3: { $OUT = 1+2 }

This line should have several numbers:
{ foreach $n (1 .. 20) { $OUT .= $n . ' ' } }
};

$c = new Safe;

# Build templates from string
$template = new Text::Template ('type' => 'STRING', 'source' => $template,
			       SAFE => $c)
  or die;
$templateOUT = new Text::Template ('type' => 'STRING', 'source' => $templateOUT,
				  SAFE => $c)
  or die;

# Fill in templates
$text = $template->fill_in()
  or die;
$textOUT = $templateOUT->fill_in()
  or die;

# (1) They should be the same
print +($text eq $textOUT ? '' : 'not '), "ok $n\n";
$n++;

# (2-3)  "Joel Appelbaum" <joel@orbz.com> <000701c0ac2c$aed1d6e0$0201a8c0@prime>
# "Contrary to the documentation the $OUT variable is not always
# undefined at the start of each program fragment.  The $OUT variable
# is never undefined after it is used once if you are using the SAFE
# option.  The result is that every fragment after the fragment that
# $OUT was used in is replaced by the old $OUT value instead of the
# result of the fragment.  This holds true even after the
# Text::Template object goes out of scope and a new one is created!"
#
# Also reported by Daini Xie.

{
  my $template = q{{$OUT = 'x'}y{$OUT .= 'z'}};
  my $expected = "xyz";
  my $s = Safe->new;
  my $o = Text::Template->new(type => 'string',
                              source => $template,
                             );
  for (1..2) {
    my $r = $o->fill_in(SAFE => $s);
    if ($r ne $expected) {
      print "not ok $n # <$r>\n";
    } else {
      print "ok $n\n";
    }
    $n++;
  }
}

exit;

