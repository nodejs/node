#!perl
# test apparatus for Text::Template module

use Text::Template;

print "1..5\n";

$n=1;

die "This is the test program for Text::Template version 1.46.
You are using version $Text::Template::VERSION instead.
That does not make sense.\n
Aborting"
  unless $Text::Template::VERSION == 1.46;

# (1) basic error delivery
{ my $r = Text::Template->new(TYPE => 'string',
                              SOURCE => '{1/0}',
                             )->fill_in();
  if ($r eq q{Program fragment delivered error ``Illegal division by zero at template line 1.''}) {
    print "ok $n\n";
  } else {
    print "not ok $n\n# $r\n";
  }
  $n++;
}

# (2) BROKEN sub called in ->new?
{ my $r = Text::Template->new(TYPE => 'string',
                              SOURCE => '{1/0}',
                              BROKEN => sub {'---'},
                             )->fill_in();
  if ($r eq q{---}) {
    print "ok $n\n";
  } else {
    print "not ok $n\n# $r\n";
  }
  $n++;
}

# (3) BROKEN sub called in ->fill_in?
{ my $r = Text::Template->new(TYPE => 'string',
                              SOURCE => '{1/0}',
                             )->fill_in(BROKEN => sub {'---'});
  if ($r eq q{---}) {
    print "ok $n\n";
  } else {
    print "not ok $n\n# $r\n";
  }
  $n++;
}

# (4) BROKEN sub passed correct args when called in ->new?
{ my $r = Text::Template->new(TYPE => 'string',
                              SOURCE => '{1/0}',
                              BROKEN => sub { my %a = @_;
                                qq{$a{lineno},$a{error},$a{text}}
                              },
                             )->fill_in();
  if ($r eq qq{1,Illegal division by zero at template line 1.\n,1/0}) {
    print "ok $n\n";
  } else {
    print "not ok $n\n# $r\n";
  }
  $n++;
}

# (5) BROKEN sub passed correct args when called in ->fill_in?
{ my $r = Text::Template->new(TYPE => 'string',
                              SOURCE => '{1/0}',
                             )->fill_in(BROKEN => 
                                        sub { my %a = @_;
                                              qq{$a{lineno},$a{error},$a{text}}
                                            });
  if ($r eq qq{1,Illegal division by zero at template line 1.\n,1/0}) {
    print "ok $n\n";
  } else {
    print "not ok $n\n# $r\n";
  }
  $n++;
}

