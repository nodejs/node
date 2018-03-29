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


print "1..12\n";

$n=1;

$template = 'We will put value of $v (which is "good") here -> {$v}';

$v = 'oops (main)';
$Q::v = 'oops (Q)';

$vars = { 'v' => \'good' };

# (1) Build template from string
$template = new Text::Template ('type' => 'STRING', 'source' => $template);
print +($template ? '' : 'not '), "ok $n\n";
$n++;

# (2) Fill in template in anonymous package
$result2 = 'We will put value of $v (which is "good") here -> good';
$text = $template->fill_in(HASH => $vars);
print +($text eq $result2 ? '' : 'not '), "ok $n\n";
$n++;

# (3) Did we clobber the main variable?
print +($v eq 'oops (main)' ? '' : 'not '), "ok $n\n";
$n++;

# (4) Fill in same template again
$result4 = 'We will put value of $v (which is "good") here -> good';
$text = $template->fill_in(HASH => $vars);
print +($text eq $result4 ? '' : 'not '), "ok $n\n";
$n++;

# (5) Now with a package
$result5 = 'We will put value of $v (which is "good") here -> good';
$text = $template->fill_in(HASH => $vars, PACKAGE => 'Q');
print +($text eq $result5 ? '' : 'not '), "ok $n\n";
$n++;

# (6) We expect to have clobbered the Q variable.
print +($Q::v eq 'good' ? '' : 'not '), "ok $n\n";
$n++;

# (7) Now let's try it without a package
$result7 = 'We will put value of $v (which is "good") here -> good';
$text = $template->fill_in(HASH => $vars);
print +($text eq $result7 ? '' : 'not '), "ok $n\n";
$n++;

# (8-11) Now what does it do when we pass a hash with undefined values?
# Roy says it does something bad. (Added for 1.20.)
my $WARNINGS = 0;
{
  local $SIG{__WARN__} = sub {$WARNINGS++};
  local $^W = 1;       # Make sure this is on for this test
  $template8 = 'We will put value of $v (which is "good") here -> {defined $v ? "bad" : "good"}';
  $result8 = 'We will put value of $v (which is "good") here -> good';
  my $template = 
    new Text::Template ('type' => 'STRING', 'source' => $template8);
  my $text = $template->fill_in(HASH => {'v' => undef});
  # (8) Did we generate a warning?
  print +($WARNINGS == 0 ? '' : 'not '), "ok $n\n";
  $n++;
  
  # (9) Was the output correct?
  print +($text eq $result8 ? '' : 'not '), "ok $n\n";
  $n++;

  # (10-11) Let's try that again, with a twist this time
  $WARNINGS = 0;
  $text = $template->fill_in(HASH => [{'v' => 17}, {'v' => undef}]);
  # (10) Did we generate a warning?
  print +($WARNINGS == 0 ? '' : 'not '), "ok $n\n";
  $n++;
  
  # (11) Was the output correct?
  if ($] < 5.005) {
    print "ok $n # skipped -- not supported before 5.005\n";
  } else {
    print +($text eq $result8 ? '' : 'not '), "ok $n\n";
  }
  $n++;
}


# (12) Now we'll test the multiple-hash option  (Added for 1.20.)
$text = Text::Template::fill_in_string(q{$v: {$v}.  @v: [{"@v"}].},
				       HASH => [{'v' => 17}, 
						{'v' => ['a', 'b', 'c']},
						{'v' => \23},
					       ]);
$result = q{$v: 23.  @v: [a b c].};
print +($text eq $result ? '' : 'not '), "ok $n\n";
$n++;


exit;

