#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
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

plan tests => 1;

if ($no_fips) {
    ok(run(test(["threadstest", "-config", $config_path, data_dir()])),
       "running test_threads");
} else {
    ok(run(test(["threadstest", "-fips", "-config", $config_path, data_dir()])),
       "running test_threads with FIPS");
}
