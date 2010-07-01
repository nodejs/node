# git log --pretty='format:%ae %an' | tail -r |  awk -f updateAuthors.awk
{
  if (!x[$1]++) {
    #print $0
    n = split($0, a, " ");
    s = a[2];
    for (i = 3; i <= n ; i++) {
      s = s " " a[i];
    }
    print s " <" $1 ">";
  }
}

