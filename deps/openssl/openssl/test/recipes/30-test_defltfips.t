#! /usr/bin/env perl
# Copyright 2015-2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test qw/:DEFAULT srctop_file srctop_dir bldtop_file bldtop_dir data_dir/;
use OpenSSL::Test::Utils;
use Cwd qw(abs_path);

BEGIN {
    setup("test_defltfips");
}

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

plan skip_all => "Configuration loading is turned off"
    if disabled("autoload-config");

my $no_fips = disabled('fips') || disabled('fips-post') || ($ENV{NO_FIPS} // 0);

plan tests =>
    ($no_fips ? 1 : 5);

unless ($no_fips) {
    $ENV{OPENSSL_CONF} = abs_path(srctop_file("test", "fips.cnf"));
    ok(run(test(["defltfips_test", "fips"])), "running defltfips_test fips");

    #Test an alternative way of configuring fips
    $ENV{OPENSSL_CONF} = abs_path(srctop_file("test", "fips-alt.cnf"));
    ok(run(test(["defltfips_test", "fips"])), "running defltfips_test fips");

    #Configured to run FIPS but the module-mac is bad
    $ENV{OPENSSL_CONF} = abs_path(srctop_file("test", "fips.cnf"));
    $ENV{OPENSSL_CONF_INCLUDE} = srctop_dir("test", "recipes", "30-test_defltfips");
    ok(run(test(["defltfips_test", "badfips"])), "running defltfips_test badfips");

    #Test an alternative way of configuring fips (but still with bad module-mac)
    $ENV{OPENSSL_CONF} = abs_path(srctop_file("test", "fips-alt.cnf"));
    ok(run(test(["defltfips_test", "badfips"])), "running defltfips_test badfips");
}

$ENV{OPENSSL_CONF} = abs_path(srctop_file("test", "default.cnf"));
ok(run(test(["defltfips_test"])), "running defltfips_test");
