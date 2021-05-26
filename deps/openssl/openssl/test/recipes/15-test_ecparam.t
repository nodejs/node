#! /usr/bin/env perl
# Copyright 2017-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use File::Compare qw/compare_text/;
use OpenSSL::Glob;
use OpenSSL::Test qw/:DEFAULT data_file/;
use OpenSSL::Test::Utils;

setup("test_ecparam");

plan skip_all => "EC or EC2M isn't supported in this build"
    if disabled("ec") || disabled("ec2m");

my @valid = glob(data_file("valid", "*.pem"));
my @noncanon = glob(data_file("noncanon", "*.pem"));
my @invalid = glob(data_file("invalid", "*.pem"));

plan tests => 11;

sub checkload {
    my $files = shift; # List of files
    my $valid = shift; # Check should pass or fail?
    my $app = shift;   # Which application
    my $opt = shift;   # Additional option

    foreach (@$files) {
        if ($valid) {
            ok(run(app(['openssl', $app, '-noout', $opt, '-in', $_])));
        } else {
            ok(!run(app(['openssl', $app, '-noout', $opt, '-in', $_])));
        }
    }
}

sub checkcompare {
    my $files = shift; # List of files
    my $app = shift;   # Which application

    foreach (@$files) {
        my $testout = "$app.tst";

        ok(run(app(['openssl', $app, '-out', $testout, '-in', $_])));
        ok(!compare_text($_, $testout), "Original file $_ is the same as new one");
    }
}

subtest "Check loading valid parameters by ecparam with -check" => sub {
    plan tests => scalar(@valid);
    checkload(\@valid, 1, "ecparam", "-check");
};

subtest "Check loading valid parameters by ecparam with -check_named" => sub {
    plan tests => scalar(@valid);
    checkload(\@valid, 1, "ecparam", "-check_named");
};

subtest "Check loading valid parameters by pkeyparam with -check" => sub {
    plan tests => scalar(@valid);
    checkload(\@valid, 1, "pkeyparam", "-check");
};

subtest "Check loading non-canonically encoded parameters by ecparam with -check" => sub {
    plan tests => scalar(@noncanon);
    checkload(\@noncanon, 1, "ecparam", "-check");
};

subtest "Check loading non-canonically encoded parameters by ecparam with -check_named" => sub {
    plan tests => scalar(@noncanon);
    checkload(\@noncanon, 1, "ecparam", "-check_named");
};

subtest "Check loading non-canonically encoded parameters by pkeyparam with -check" => sub {
    plan tests => scalar(@noncanon);
    checkload(\@noncanon, 1, "pkeyparam", "-check");
};

subtest "Check loading invalid parameters by ecparam with -check" => sub {
    plan tests => scalar(@invalid);
    checkload(\@invalid, 0, "ecparam", "-check");
};

subtest "Check loading invalid parameters by ecparam with -check_named" => sub {
    plan tests => scalar(@invalid);
    checkload(\@invalid, 0, "ecparam", "-check_named");
};

subtest "Check loading invalid parameters by pkeyparam with -check" => sub {
    plan tests => scalar(@invalid);
    checkload(\@invalid, 0, "pkeyparam", "-check");
};

subtest "Check ecparam does not change the parameter file on output" => sub {
    plan tests => 2 * scalar(@valid);
    checkcompare(\@valid, "ecparam");
};

subtest "Check pkeyparam does not change the parameter file on output" => sub {
    plan tests => 2 * scalar(@valid);
    checkcompare(\@valid, "pkeyparam");
};
