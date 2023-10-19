#! /usr/bin/env perl
# Copyright 2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

# This is a sanity checker to see that the fipsmodule.cnf that's been
# generated for testing is valid.

use strict;
use warnings;

use OpenSSL::Test qw/:DEFAULT srctop_dir bldtop_dir bldtop_file srctop_file data_file/;
use OpenSSL::Test::Utils;

BEGIN {
    setup("test_fipsmodule");
}

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');
use platform;

my $no_check = disabled("fips");
plan skip_all => "Test only supported in a fips build"
    if $no_check;
plan tests => 1;

my $fipsmodule = bldtop_file('providers', platform->dso('fips'));
my $fipsmoduleconf = bldtop_file('test', 'fipsmodule.cnf');

# verify the $fipsconf file
ok(run(app(['openssl', 'fipsinstall',
            '-in',  $fipsmoduleconf, '-module', $fipsmodule, '-verify'])),
   "fipsinstall verify");
