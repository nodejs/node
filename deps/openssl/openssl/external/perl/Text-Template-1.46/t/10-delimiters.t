#!perl
#
# Tests for user-specified delimiter functions
# These tests first appeared in version 1.20.

use Text::Template;

die "This is the test program for Text::Template version 1.46.
You are using version $Text::Template::VERSION instead.
That does not make sense.\n
Aborting"
  unless $Text::Template::VERSION == 1.46;

print "1..18\n";
$n = 1;

# (1) Try a simple delimiter: <<..>>
# First with the delimiters specified at object creation time
$V = $V = 119;
$template = q{The value of $V is <<$V>>.};
$result = q{The value of $V is 119.};
$template1 = Text::Template->new(TYPE => STRING, 
				 SOURCE => $template,
				 DELIMITERS => ['<<', '>>']
				)
  or die "Couldn't construct template object: $Text::Template::ERROR; aborting";
$text = $template1->fill_in();
print +($text eq $result ? '' : 'not '), "ok $n\n";
$n++;

# (2) Now with delimiter choice deferred until fill-in time.
$template1 = Text::Template->new(TYPE => STRING, SOURCE => $template);
$text = $template1->fill_in(DELIMITERS => ['<<', '>>']);
print +($text eq $result ? '' : 'not '), "ok $n\n";
$n++;

# (3) Now we'll try using regex metacharacters
# First with the delimiters specified at object creation time
$template = q{The value of $V is [$V].};
$template1 = Text::Template->new(TYPE => STRING, 
				 SOURCE => $template,
				 DELIMITERS => ['[', ']']
				)
  or die "Couldn't construct template object: $Text::Template::ERROR; aborting";
$text = $template1->fill_in();
print +($text eq $result ? '' : 'not '), "ok $n\n";
$n++;

# (4) Now with delimiter choice deferred until fill-in time.
$template1 = Text::Template->new(TYPE => STRING, SOURCE => $template);
$text = $template1->fill_in(DELIMITERS => ['[', ']']);
print +($text eq $result ? '' : 'not '), "ok $n\n";
$n++;



# (5-18) Make sure \ is working properly
# (That is to say, it is ignored.)
# These tests are similar to those in 01-basic.t.
my @tests = ('{""}' => '',	# (5)

	     # Backslashes don't matter
	     '{"}"}' => undef,
	     '{"\\}"}' => undef,	# One backslash
	     '{"\\\\}"}' => undef, # Two backslashes
	     '{"\\\\\\}"}' => undef, # Three backslashes 
	     '{"\\\\\\\\}"}' => undef, # Four backslashes (10)
	     '{"\\\\\\\\\\}"}' => undef, # Five backslashes
	     
	     # Backslashes are always passed directly to Perl
	     '{"x20"}' => 'x20',
	     '{"\\x20"}' => ' ',	# One backslash
	     '{"\\\\x20"}' => '\\x20', # Two backslashes
	     '{"\\\\\\x20"}' => '\\ ', # Three backslashes (15)
	     '{"\\\\\\\\x20"}' => '\\\\x20', # Four backslashes
	     '{"\\\\\\\\\\x20"}' => '\\\\ ', # Five backslashes
	     '{"\\x20\\}"}' => undef, # (18)
	    );

my $i;
for ($i=0; $i<@tests; $i+=2) {
  my $tmpl = Text::Template->new(TYPE => 'STRING',
				 SOURCE => $tests[$i],
				 DELIMITERS => ['{', '}'],
				);
  my $text = $tmpl->fill_in;
  my $result = $tests[$i+1];
  my $ok = (! defined $text && ! defined $result
	    || $text eq $result);
  unless ($ok) {
    print STDERR "($n) expected .$result., got .$text.\n";
  }
  print +($ok ? '' : 'not '), "ok $n\n";
  $n++;
}


exit;

