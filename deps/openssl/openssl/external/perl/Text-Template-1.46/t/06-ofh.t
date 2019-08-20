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

print "1..2\n";

$n=1;

$template = new Text::Template TYPE => STRING, SOURCE => q{My process ID is {$$}};
$of = "t$$";
END { unlink $of }
open O, "> $of" or die;

$text = $template->fill_in(OUTPUT => \*O);

# (1) No $text should have been constructed.  Return value should be true.
print +($text eq '1' ? '' : 'not '), "ok $n\n";
$n++;

close O or die;
open I, "< $of" or die;
{ local $/; $t = <I> }
close I;

# (2) The text should have been printed to the file
print +($t eq "My process ID is $$" ? '' : 'not '), "ok $n\n";
$n++;

exit;

