#!/usr/local/bin/perl
#
# tack error codes onto the end of a file
#

open(ERR,$ARGV[0]) || die "unable to open error file '$ARGV[0]':$!\n";
@err=<ERR>;
close(ERR);

open(IN,$ARGV[1]) || die "unable to open header file '$ARGV[1]':$!\n";

@out="";
while (<IN>)
	{
	push(@out,$_);
	last if /BEGIN ERROR CODES/;
	}
close(IN);

open(OUT,">$ARGV[1]") || die "unable to open header file '$ARGV[1]':$1\n";
print OUT @out;
print OUT @err;
print OUT <<"EOF";
 
#ifdef  __cplusplus
}
#endif
#endif

EOF
close(OUT);


