#!/bin/sh

git log --reverse --format='%aN <%aE>' | perl -we '

BEGIN {
  %seen = (), @authors = ();
}

while (<>) {
  next if $seen{$_};
  $seen{$_} = push @authors, $_;
}

END {
  print "# Authors sorted by whether or not they'\''re me\n";
  print @authors;
}
' > AUTHORS
