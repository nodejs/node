#! /usr/bin/env perl
# Copyright 2016-2019 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test qw/:DEFAULT bldtop_dir bldtop_file/;
use OpenSSL::Test::Utils;
use File::Temp qw(tempfile);

#Load configdata.pm

BEGIN {
    setup("test_shlibload");
}
use lib bldtop_dir('.');
use configdata;

plan skip_all => "Test only supported in a shared build" if disabled("shared");
plan skip_all => "Test is disabled on AIX" if config('target') =~ m|^aix|;
plan skip_all => "Test is disabled on VMS" if config('target') =~ m|^vms|;
plan skip_all => "Test only supported in a dso build" if disabled("dso");

plan tests => 10;

# When libssl and libcrypto are compiled on Linux with "-rpath", but not
# "--enable-new-dtags", the RPATH takes precedence over LD_LIBRARY_PATH,
# and we end up running with the wrong libraries.  This is resolved by
# using paths to the shared objects, not just the names.

my $libcrypto = bldtop_file(shlib('libcrypto'));
my $libssl = bldtop_file(shlib('libssl'));

(my $fh, my $filename) = tempfile();
ok(run(test(["shlibloadtest", "-crypto_first", $libcrypto, $libssl, $filename])),
   "running shlibloadtest -crypto_first $filename");
ok(check_atexit($fh));
unlink $filename;
($fh, $filename) = tempfile();
ok(run(test(["shlibloadtest", "-ssl_first", $libcrypto, $libssl, $filename])),
   "running shlibloadtest -ssl_first $filename");
ok(check_atexit($fh));
unlink $filename;
($fh, $filename) = tempfile();
ok(run(test(["shlibloadtest", "-just_crypto", $libcrypto, $libssl, $filename])),
   "running shlibloadtest -just_crypto $filename");
ok(check_atexit($fh));
unlink $filename;
($fh, $filename) = tempfile();
ok(run(test(["shlibloadtest", "-dso_ref", $libcrypto, $libssl, $filename])),
   "running shlibloadtest -dso_ref $filename");
ok(check_atexit($fh));
unlink $filename;
($fh, $filename) = tempfile();
ok(run(test(["shlibloadtest", "-no_atexit", $libcrypto, $libssl, $filename])),
   "running shlibloadtest -no_atexit $filename");
ok(!check_atexit($fh));
unlink $filename;

sub shlib {
    my $lib = shift;
    $lib = $unified_info{rename}->{$lib}
        if defined $unified_info{rename}->{$lib};
    $lib = $unified_info{sharednames}->{$lib}
        . ($target{shlib_variant} || "")
        . ($target{shared_extension} || ".so");
    $lib =~ s|\.\$\(SHLIB_VERSION_NUMBER\)
             |.$config{shlib_version_number}|x;
    return $lib;
}

sub check_atexit {
    my $fh = shift;
    my $data = <$fh>;

    return 1 if (defined $data && $data =~ m/atexit\(\) run/);

    return 0;
}
