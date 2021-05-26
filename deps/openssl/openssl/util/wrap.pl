#! /usr/bin/env perl

use strict;
use warnings;

use File::Basename;
use File::Spec::Functions;

my $there = canonpath(catdir(dirname($0), updir()));
my $std_engines = catdir($there, 'engines');
my $std_providers = catdir($there, 'providers');
my $std_openssl_conf = catdir($there, 'apps/openssl.cnf');
my $unix_shlib_wrap = catfile($there, 'util/shlib_wrap.sh');

$ENV{OPENSSL_ENGINES} = $std_engines
    if ($ENV{OPENSSL_ENGINES} // '') eq '' && -d $std_engines;
$ENV{OPENSSL_MODULES} = $std_providers
    if ($ENV{OPENSSL_MODULES} // '') eq '' && -d $std_providers;
$ENV{OPENSSL_CONF} = $std_openssl_conf
    if ($ENV{OPENSSL_CONF} // '') eq '' && -f $std_openssl_conf;

my $use_system = 0;
my @cmd;

if (-x $unix_shlib_wrap) {
    @cmd = ( $unix_shlib_wrap, @ARGV );
} else {
    # Hope for the best
    @cmd = ( @ARGV );
}

# The exec() statement on MSWin32 doesn't seem to give back the exit code
# from the call, so we resort to using system() instead.
my $waitcode = system @cmd;

# According to documentation, -1 means that system() couldn't run the command,
# otherwise, the value is similar to the Unix wait() status value
# (exitcode << 8 | signalcode)
die "wrap.pl: Failed to execute '", join(' ', @cmd), "': $!\n"
    if $waitcode == -1;

# When the subprocess aborted on a signal, mimic what Unix shells do, by
# converting the signal code to an exit code by setting the high bit.
# This only happens on Unix flavored operating systems, the others don't
# have this sort of signaling to date, and simply leave the low byte zero.
exit(($? & 255) | 128) if ($? & 255) != 0;

# When not a signal, just shift down the subprocess exit code and use that.
exit($? >> 8);
