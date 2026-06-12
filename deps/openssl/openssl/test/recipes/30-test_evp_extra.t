#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test qw/:DEFAULT bldtop_dir srctop_file/;
use OpenSSL::Test::Utils;

setup("test_evp_extra");

my $no_conf_autoload = disabled('autoload-config');

plan tests => $no_conf_autoload ? 3 : 4;

ok(run(test(["evp_extra_test"])), "running evp_extra_test");

unless ($no_conf_autoload) {
    local $ENV{OPENSSL_CONF} = srctop_file("test","default-for-evptest.cnf");
    ok(run(test(["evp_extra_test", "-config", srctop_file("test","default-for-evptest.cnf")])),
       "running evp_extra_test to test evp properties set in config");
    delete local $ENV{OPENSSL_CONF};
}

# Run tests with a non-default library context
ok(run(test(["evp_extra_test", "-context"])), "running evp_extra_test with a non-default library context");

ok(run(test(["evp_extra_test2"])), "running evp_extra_test2");
