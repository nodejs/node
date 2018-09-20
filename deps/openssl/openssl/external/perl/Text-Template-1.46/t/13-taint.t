#!perl -T
# Tests for taint-mode features

use lib 'blib/lib';
use Text::Template;

die "This is the test program for Text::Template version 1.46.
You are using version $Text::Template::VERSION instead.
That does not make sense.\n
Aborting"
  unless $Text::Template::VERSION == 1.46;

my $r = int(rand(10000));
my $file = "tt$r";

# makes its arguments tainted
sub taint {
  for (@_) {
    $_ .= substr($0,0,0);       # LOD
  }
}


print "1..21\n";

my $n =1;
print "ok ", $n++, "\n";

my $template = 'The value of $n is {$n}.';

open T, "> $file" or die "Couldn't write temporary file $file: $!";
print T $template, "\n";
close T or die "Couldn't finish temporary file $file: $!";

sub should_fail {
  my $obj = Text::Template->new(@_);
  eval {$obj->fill_in()};
  if ($@) {
    print "ok $n # $@\n";
  } else {
    print "not ok $n # (didn't fail)\n";
  }
  $n++;
}

sub should_work {
  my $obj = Text::Template->new(@_);
  eval {$obj->fill_in()};
  if ($@) {
    print "not ok $n # $@\n";
  } else {
    print "ok $n\n";
  }
  $n++;
}

sub should_be_tainted {
  if (Text::Template::_is_clean($_[0])) {
    print "not ok $n\n"; $n++; return;
  }
  print "ok $n\n"; $n++; return; 
}

sub should_be_clean {
  unless (Text::Template::_is_clean($_[0])) {
    print "not ok $n\n"; $n++; return;
  }
  print "ok $n\n"; $n++; return; 
}

# Tainted filename should die with and without UNTAINT option
# untainted filename should die without UNTAINT option
# filehandle should die without UNTAINT option
# string and array with tainted data should die either way

# (2)-(7)
my $tfile = $file;
taint($tfile);
should_be_tainted($tfile);
should_be_clean($file);
should_fail TYPE => 'file', SOURCE => $tfile;
should_fail TYPE => 'file', SOURCE => $tfile, UNTAINT => 1;
should_fail TYPE => 'file', SOURCE => $file;
should_work TYPE => 'file', SOURCE => $file, UNTAINT => 1;

# (8-9)
open H, "< $file" or die "Couldn't open $file for reading: $!; aborting";
should_fail TYPE => 'filehandle', SOURCE => \*H;
close H;
open H, "< $file" or die "Couldn't open $file for reading: $!; aborting";
should_work TYPE => 'filehandle', SOURCE => \*H, UNTAINT => 1;
close H;

# (10-15)
my $ttemplate = $template;
taint($ttemplate);
should_be_tainted($ttemplate);
should_be_clean($template);
should_fail TYPE => 'string', SOURCE => $ttemplate;
should_fail TYPE => 'string', SOURCE => $ttemplate, UNTAINT => 1;
should_work TYPE => 'string', SOURCE => $template;
should_work TYPE => 'string', SOURCE => $template, UNTAINT => 1;

# (16-19)
my $array = [ $template ];
my $tarray = [ $ttemplate ];
should_fail TYPE => 'array', SOURCE => $tarray;
should_fail TYPE => 'array', SOURCE => $tarray, UNTAINT => 1;
should_work TYPE => 'array', SOURCE => $array;
should_work TYPE => 'array', SOURCE => $array, UNTAINT => 1;

# (20-21) Test _unconditionally_untaint utility function
Text::Template::_unconditionally_untaint($ttemplate);
should_be_clean($ttemplate);
Text::Template::_unconditionally_untaint($tfile);
should_be_clean($tfile);

END { unlink $file }

