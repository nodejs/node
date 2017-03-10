#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_sid");

plan tests => 2;

require_ok(srctop_file('test','recipes','tconversion.pl'));

subtest 'sid conversions' => sub {
    tconversion("sid", srctop_file("test","testsid.pem"), "sess_id");
};
