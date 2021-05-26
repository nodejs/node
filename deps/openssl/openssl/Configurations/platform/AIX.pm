package platform::AIX;

use strict;
use warnings;
use Carp;

use vars qw(@ISA);

require platform::Unix;
@ISA = qw(platform::Unix);

# Assume someone set @INC right before loading this module
use configdata;

sub dsoext              { '.so' }
sub shlibextsimple      { '.a' }

# In shared mode, the default static library names clashes with the final
# "simple" full shared library name, so we add '_a' to the basename of the
# static libraries in that case.
sub staticname {
    # Non-installed libraries are *always* static, and their names remain
    # the same, except for the mandatory extension
    my $in_libname = platform::BASE->staticname($_[1]);
    return $in_libname if $unified_info{attributes}->{$_[1]}->{noinst};

    return platform::BASE->staticname($_[1]) . '_a';
}
