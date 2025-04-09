#! /usr/bin/env perl
# Copyright 2015-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test::Simple;
use OpenSSL::Test qw/:DEFAULT srctop_file srctop_dir bldtop_dir bldtop_file data_dir/;
use OpenSSL::Test::Utils;
use Cwd qw(abs_path);

BEGIN {
setup("test_threads");
}

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);
my $config_path = abs_path(srctop_file("test", $no_fips ? "default.cnf"
                                                        : "default-and-fips.cnf"));

plan tests => 3;

if ($no_fips) {
    ok(run(test(["threadstest", "-config", $config_path, data_dir()])),
       "running test_threads");
} else {
    ok(run(test(["threadstest", "-fips", "-config", $config_path, data_dir()])),
       "running test_threads with FIPS");
}

ok(run(test(["threadpool_test"])), "running threadpool_test");

# Merge the configuration files into one filtering the contents so the failure
# condition is reproducible.  A working FIPS configuration without the install
# status is required.

open CFGBASE, '<', $config_path;
open CFGINC, '<', bldtop_file('/test/fipsmodule.cnf');
open CFGOUT, '>', 'thread.cnf';

while (<CFGBASE>) {
    print CFGOUT unless m/^[.]include/;
}
close CFGBASE;
print CFGOUT "\n\n";
while (<CFGINC>) {
    print CFGOUT unless m/^install-status/;
}
close CFGINC;
close CFGOUT;

$ENV{OPENSSL_CONF} = 'thread.cnf';
ok(run(test(["threadstest_fips"])), "running threadstest_fips");
