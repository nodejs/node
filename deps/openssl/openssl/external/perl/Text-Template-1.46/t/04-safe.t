#!perl
#
# test apparatus for Text::Template module
# still incomplete.

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

print "1..16\n";

if ($^O eq 'MacOS') {
  $BADOP = qq{};
  $FAILURE = q{};
} else {
  $BADOP = qq{kill 0};
  $FAILURE = q{Program fragment at line 1 delivered error ``kill trapped by operation mask''};
}

$n=1;
$v = $v = 119;

$c = new Safe or die;

$goodtemplate = q{This should succeed: { $v }};
$goodoutput   = q{This should succeed: 119};

$template1 = new Text::Template ('type' => 'STRING', 'source' => $goodtemplate)
  or die;
$template2 = new Text::Template ('type' => 'STRING', 'source' => $goodtemplate)
  or die;

$text1 = $template1->fill_in();
$text2 = $template1->fill_in(SAFE => $c);
$ERR2 = $@;
$text3 = $template2->fill_in(SAFE => $c);
$ERR3 = $@;

# (1)(2)(3) None of these should have failed.
print +(defined $text1 ? '' : 'not '), "ok $n\n";
$n++;
print +(defined $text2 ? '' : 'not '), "ok $n\n";
$n++;
print +(defined $text3 ? '' : 'not '), "ok $n\n";
$n++;

# (4) Safe and non-safe fills of different template objects with the
# same template text should yield the same result.
# print +($text1 eq $text3 ? '' : 'not '), "ok $n\n";
# (4) voided this test:  it's not true, because the unsafe fill
# uses package main, while the safe fill uses the secret safe package.
# We could alias the secret safe package to be identical to main,
# but that wouldn't be safe.  If you want the aliasing, you have to
# request it explicitly with `PACKAGE'.
print "ok $n\n";
$n++;

# (5) Safe and non-safe fills of the same template object
# should yield the same result.
# (5) voided this test for the same reason as #4.
# print +($text1 eq $text2 ? '' : 'not '), "ok $n\n";
print "ok $n\n";
$n++;

# (6) Make sure the output was actually correct
print +($text1 eq $goodoutput ? '' : 'not '), "ok $n\n";
$n++;


$badtemplate     = qq{This should fail: { $BADOP; 'NOFAIL' }};
$badnosafeoutput = q{This should fail: NOFAIL};
$badsafeoutput   = q{This should fail: Program fragment delivered error ``kill trapped by operation mask at template line 1.''};

$template1 = new Text::Template ('type' => 'STRING', 'source' => $badtemplate)
  or die;
$template2 = new Text::Template ('type' => 'STRING', 'source' => $badtemplate)
  or die;

$text1 = $template1->fill_in();
$text2 = $template1->fill_in(SAFE => $c);
$ERR2 = $@;
$text3 = $template2->fill_in(SAFE => $c);
$ERR3 = $@;
$text4 = $template1->fill_in();

# (7)(8)(9)(10) None of these should have failed.
print +(defined $text1 ? '' : 'not '), "ok $n\n";
$n++;
print +(defined $text2 ? '' : 'not '), "ok $n\n";
$n++;
print +(defined $text3 ? '' : 'not '), "ok $n\n";
$n++;
print +(defined $text4 ? '' : 'not '), "ok $n\n";
$n++;

# (11) text1 and text4 should be the same (using safe in between
# didn't change anything.)
print +($text1 eq $text4 ? '' : 'not '), "ok $n\n";
$n++;

# (12) text2 and text3 should be the same (same template text in different
# objects
print +($text2 eq $text3 ? '' : 'not '), "ok $n\n";
$n++;

# (13) text1 should yield badnosafeoutput
print +($text1 eq $badnosafeoutput ? '' : 'not '), "ok $n\n";
$n++;

# (14) text2 should yield badsafeoutput
$text2 =~ s/'kill'/kill/;  # 5.8.1 added quote marks around the op name
print "# expected: <$badsafeoutput>\n# got     : <$text2>\n";
print +($text2 eq $badsafeoutput ? '' : 'not '), "ok $n\n";
$n++;


$template = q{{$x=1}{$x+1}};

$template1 = new Text::Template ('type' => 'STRING', 'source' => $template)
  or die;
$template2 = new Text::Template ('type' => 'STRING', 'source' => $template)
  or die;

$text1 = $template1->fill_in();
$text2 = $template1->fill_in(SAFE => new Safe);

# (15) Do effects persist in safe compartments?
print +($text1 eq $text2 ? '' : 'not '), "ok $n\n";
$n++;

# (16) Try the BROKEN routine in safe compartments
sub my_broken { 
  my %a = @_; $a{error} =~ s/ at.*//s;
  "OK! text:$a{text} error:$a{error} lineno:$a{lineno} arg:$a{arg}" ;
}
$templateB = new Text::Template (TYPE => 'STRING', SOURCE => '{die}')
    or die;
$text1 = $templateB->fill_in(BROKEN => \&my_broken, 
			     BROKEN_ARG => 'barg',
			     SAFE => new Safe,
			     );
$result1 = qq{OK! text:die error:Died lineno:1 arg:barg};
print +($text1 eq $result1 ? '' : 'not '), "ok $n\n";
$n++;



exit;

