#!/bin/sh

git log --reverse --format='%aN <%aE>' | awk '
BEGIN {
  print "# Authors sorted by whether or not they'\''re me";
}

{
  if (all[$NF] != 1) {
    all[$NF] = 1;
    ordered[length(all)] = $0;
  }
}

END {
  for (i in ordered) {
    print ordered[i];
  }
}
' > AUTHORS