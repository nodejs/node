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

print "1..12\n";
$n = 1;

$c = new Safe or die;

# Test handling of packages and importing.
$c->reval('$P = "safe root"');
$P = $P = 'main';
$Q::P = $Q::P = 'Q';

# How to effectively test the gensymming?

$t = new Text::Template TYPE => 'STRING', SOURCE => 'package is {$P}'
    or die;

# (1) Default behavior: Inherit from calling package, `main' in this case.
$text = $t->fill_in();
print +($text eq 'package is main' ? '' : 'not '), "ok $n\n";
$n++;

# (2) When a package is specified, we should use that package instead.
$text = $t->fill_in(PACKAGE => 'Q');
print +($text eq 'package is Q' ? '' : 'not '), "ok $n\n";
$n++;

# (3) When no package is specified in safe mode, we should use the
# default safe root.
$text = $t->fill_in(SAFE => $c);
print +($text eq 'package is safe root' ? '' : 'not '), "ok $n\n";
$n++;

# (4) When a package is specified in safe mode, we should use the
# default safe root, after aliasing to the specified package
$text = $t->fill_in(SAFE => $c, PACKAGE => Q);
print +($text eq 'package is Q' ? '' : 'not '), "ok $n\n";
$n++;

# Now let's see if hash vars are installed properly into safe templates
$t = new Text::Template TYPE => 'STRING', SOURCE => 'hash is {$H}'
    or die;

# (5) First in default mode
$text = $t->fill_in(HASH => {H => 'good5'} );
print +($text eq 'hash is good5' ? '' : 'not '), "ok $n\n";
$n++;

# (6) Now in packages
$text = $t->fill_in(HASH => {H => 'good6'}, PACKAGE => 'Q' );
print +($text eq 'hash is good6' ? '' : 'not '), "ok $n\n";
$n++;

# (7) Now in the default root of the safe compartment
$text = $t->fill_in(HASH => {H => 'good7'}, SAFE => $c );
print +($text eq 'hash is good7' ? '' : 'not '), "ok $n\n";
$n++;

# (8) Now in the default root after aliasing to a package that
# got the hash stuffed in
$text = $t->fill_in(HASH => {H => 'good8'}, SAFE => $c, PACKAGE => 'Q2' );
print +($text eq 'hash is good8' ? '' : 'not '), "ok $n\n";
$n++;

# Now let's make sure that none of the packages leaked on each other.
# (9) This var should NOT have been installed into the main package
print +(defined $H ? 'not ' : ''), "ok $n\n";
$H=$H;
$n++;

# (10) good6 was overwritten in test 7, so there's nothing to test for here.
print "ok $n\n";
$n++;

# (11) this value overwrote the one from test 6.
print +($Q::H eq 'good7' ? '' : 'not '), "ok $n\n";
$Q::H = $Q::H;
$n++;

# (12) 
print +($Q2::H eq 'good8' ? '' : 'not '), "ok $n\n";
$Q2::H = $Q2::H;
$n++;



