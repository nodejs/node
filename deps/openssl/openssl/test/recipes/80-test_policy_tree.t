#! /usr/bin/env perl
# Copyright 2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use POSIX;
use OpenSSL::Test qw/:DEFAULT srctop_file with data_file/;

use OpenSSL::Test::Utils;
use OpenSSL::Glob;

setup("test_policy_tree");

plan skip_all => "No EC support" if disabled("ec");

plan tests => 2;

# The small pathological tree is expected to work
my $small_chain = srctop_file("test", "recipes", "80-test_policy_tree_data",
                              "small_policy_tree.pem");
my $small_leaf = srctop_file("test", "recipes", "80-test_policy_tree_data",
                             "small_leaf.pem");

ok(run(app(["openssl", "verify", "-CAfile", $small_chain,
            "-policy_check", $small_leaf])),
   "test small policy tree");

# The large pathological tree is expected to fail
my $large_chain = srctop_file("test", "recipes", "80-test_policy_tree_data",
                              "large_policy_tree.pem");
my $large_leaf = srctop_file("test", "recipes", "80-test_policy_tree_data",
                             "large_leaf.pem");

ok(!run(app(["openssl", "verify", "-CAfile", $large_chain,
             "-policy_check", $large_leaf])),
   "test large policy tree");
