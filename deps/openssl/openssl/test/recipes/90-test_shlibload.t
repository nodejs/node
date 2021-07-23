#! /usr/bin/env perl
# Copyright 2016-2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test qw/:DEFAULT srctop_dir bldtop_dir/;
use OpenSSL::Test::Utils;
use File::Temp qw(tempfile);

#Load configdata.pm

BEGIN {
    setup("test_shlibload");
}
use lib srctop_dir('Configurations');
use lib bldtop_dir('.');
use platform;

plan skip_all => "Test only supported in a shared build" if disabled("shared");
plan skip_all => "Test is disabled on AIX" if config('target') =~ m|^aix|;
plan skip_all => "Test is disabled on NonStop" if config('target') =~ m|^nonstop|;
plan skip_all => "Test only supported in a dso build" if disabled("dso");
plan skip_all => "Test is disabled in an address sanitizer build" unless disabled("asan");

plan tests => 10;

my $libcrypto = platform->sharedlib('libcrypto');
my $libssl = platform->sharedlib('libssl');

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

sub check_atexit {
    my $fh = shift;
    my $data = <$fh>;

    return 1 if (defined $data && $data =~ m/atexit\(\) run/);

    return 0;
}
