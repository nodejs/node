#!perl
#
# test apparatus for Text::Template module
# still incomplete.
#

use Text::Template;

die "This is the test program for Text::Template version 1.46
You are using version $Text::Template::VERSION instead.
That does not make sense.\n
Aborting"
  unless $Text::Template::VERSION == 1.46;

print "1..1\n";

$n=1;

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

# Build templates from string
$template = new Text::Template ('type' => 'STRING', 'source' => $template)
  or die;
$templateOUT = new Text::Template ('type' => 'STRING', 'source' => $templateOUT)
  or die;

# Fill in templates
$text = $template->fill_in()
  or die;
$textOUT = $templateOUT->fill_in()
  or die;

# (1) They should be the same
print +($text eq $textOUT ? '' : 'not '), "ok $n\n";
$n++;

# Missing:  Test this feature in Safe compartments; 
# it's a totally different code path.
# Decision: Put that into safe.t, because that file should
# be skipped when Safe.pm is unavailable.


exit;

