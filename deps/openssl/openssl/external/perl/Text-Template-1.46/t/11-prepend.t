#!perl
#
# Tests for PREPEND features
# These tests first appeared in version 1.22.

use Text::Template;

die "This is the test program for Text::Template version 1.46
You are using version $Text::Template::VERSION instead.
That does not make sense.\n
Aborting"
  unless $Text::Template::VERSION == 1.46;

print "1..9\n";
my $n = 1;

@Emptyclass1::ISA = 'Text::Template';
@Emptyclass2::ISA = 'Text::Template';

my $tin = q{The value of $foo is: {$foo}};

Text::Template->always_prepend(q{$foo = "global"});

$tmpl1 = Text::Template->new(TYPE => 'STRING',
				SOURCE => $tin,		
			      );

$tmpl2 = Text::Template->new(TYPE => 'STRING',
			     SOURCE => $tin,		
			     PREPEND => q{$foo = "template"},
			     );

$tmpl1->compile;
$tmpl2->compile;

$t1 = $tmpl1->fill_in(PACKAGE => 'T1');
$t2 = $tmpl2->fill_in(PACKAGE => 'T2');
$t3 = $tmpl2->fill_in(PREPEND => q{$foo = "fillin"}, PACKAGE => 'T3');

($t1 eq 'The value of $foo is: global') or print "not ";
print "ok $n\n"; $n++;
($t2 eq 'The value of $foo is: template') or print "not ";
print "ok $n\n"; $n++;
($t3 eq 'The value of $foo is: fillin') or print "not ";
print "ok $n\n"; $n++;

Emptyclass1->always_prepend(q{$foo = 'Emptyclass global';});
$tmpl1 = Emptyclass1->new(TYPE => 'STRING',
				SOURCE => $tin,		
			      );

$tmpl2 = Emptyclass1->new(TYPE => 'STRING',
			     SOURCE => $tin,		
			     PREPEND => q{$foo = "template"},
			     );

$tmpl1->compile;
$tmpl2->compile;

$t1 = $tmpl1->fill_in(PACKAGE => 'T4');
$t2 = $tmpl2->fill_in(PACKAGE => 'T5');
$t3 = $tmpl2->fill_in(PREPEND => q{$foo = "fillin"}, PACKAGE => 'T6');

($t1 eq 'The value of $foo is: Emptyclass global') or print "not ";
print "ok $n\n"; $n++;
($t2 eq 'The value of $foo is: template') or print "not ";
print "ok $n\n"; $n++;
($t3 eq 'The value of $foo is: fillin') or print "not ";
print "ok $n\n"; $n++;

$tmpl1 = Emptyclass2->new(TYPE => 'STRING',
				SOURCE => $tin,		
			      );

$tmpl2 = Emptyclass2->new(TYPE => 'STRING',
			     SOURCE => $tin,		
			     PREPEND => q{$foo = "template"},
			     );

$tmpl1->compile;
$tmpl2->compile;

$t1 = $tmpl1->fill_in(PACKAGE => 'T4');
$t2 = $tmpl2->fill_in(PACKAGE => 'T5');
$t3 = $tmpl2->fill_in(PREPEND => q{$foo = "fillin"}, PACKAGE => 'T6');

($t1 eq 'The value of $foo is: global') or print "not ";
print "ok $n\n"; $n++;
($t2 eq 'The value of $foo is: template') or print "not ";
print "ok $n\n"; $n++;
($t3 eq 'The value of $foo is: fillin') or print "not ";
print "ok $n\n"; $n++;


