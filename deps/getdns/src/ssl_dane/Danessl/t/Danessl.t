# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl Danessl.t'

#########################

# change 'tests => 2' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More tests => 2;
BEGIN { use_ok('Danessl') };


my $fail = 0;
foreach my $constname (qw(
        USAGE_PKIX_TA USAGE_PKIX_EE USAGE_DANE_TA USAGE_DANE_EE
        SELECTOR_CERT SELECTOR_SPKI
        MATCHING_FULL MATCHING_2256 MATCHING_2512)) {
  next if (eval "my \$a = $constname; 1");
  if ($@ =~ /^Your vendor has not defined Danessl macro $constname/) {
    print "# pass: $@";
  } else {
    print "# fail: $@";
    $fail = 1;
  }

}

ok( $fail == 0 , 'Constants' );
#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

