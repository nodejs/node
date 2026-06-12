#! /usr/bin/env perl
# Copyright 2020-2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_configutl");

my @tests = qw(escapes.cnf
leading-and-trailing-whitespace.cnf
order.cnf
variables.cnf);

#includes.cnf is temporary excluded as it depends on #27300

plan tests => 2 * (scalar(@tests)) + 1;

require_ok(srctop_file('test', 'recipes', 'tconversion.pl'));

foreach my $file (@tests) {
    my $path = ($file eq "includes.cnf") ? srctop_file("test", $file) :
               srctop_file("test", "recipes", "25-test_configutl_data", $file);
    ok(run(app(["openssl", "configutl",
       "-config", $path,
       "-noheader", "-out", "$file.got"])));

    if ($file eq "includes.cnf") {
        my $cmp1 = cmp_text("$file.got", srctop_file("test", "recipes", "25-test_configutl_data", "$file.expected1"));
	my $cmp2 = cmp_text("$file.got", srctop_file("test", "recipes", "25-test_configutl_data", "$file.expected2"));

	is((($cmp1 == 0) || ($cmp2 == 0)), 1, "$file got/expected 1/2");
    } else {
        is(cmp_text("$file.got",
           srctop_file("test", "recipes", "25-test_configutl_data", "$file.expected")),
           0, "$file got/expected");
    }

    unlink "$file.got";
}
