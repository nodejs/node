#! /usr/bin/env perl
# Copyright 2015-2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

setup("test_sid");

plan skip_all => 'test_sid needs EC to run'
    if disabled('ec');

plan tests => 2;

require_ok(srctop_file('test','recipes','tconversion.pl'));

subtest 'sid conversions' => sub {
    tconversion( -type => 'sid', -in => srctop_file("test","testsid.pem"),
                 -args => ["sess_id"] );
};
