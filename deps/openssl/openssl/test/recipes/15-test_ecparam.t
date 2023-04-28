#! /usr/bin/env perl
# Copyright 2017-2022 The OpenSSL Project Authors. All Rights Reserved.
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
use OpenSSL::Test qw/:DEFAULT data_file srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;

setup("test_ecparam");

plan skip_all => "EC or EC2M isn't supported in this build"
    if disabled("ec") || disabled("ec2m");

my @valid = glob(data_file("valid", "*.pem"));
my @noncanon = glob(data_file("noncanon", "*.pem"));
my @invalid = glob(data_file("invalid", "*.pem"));

plan tests => 12;

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
        ok(!compare_text($_, $testout, sub {
            my $in1 = $_[0];
            my $in2 = $_[1];
            $in1 =~ s/\r\n/\n/g;
            $in2 =~ s/\r\n/\n/g;
            $in1 ne $in2}), "Original file $_ is the same as new one");
    }
}

my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);

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

subtest "Check loading of fips and non-fips params" => sub {
    plan skip_all => "FIPS is disabled"
        if $no_fips;
    plan tests => 8;

    my $fipsconf = srctop_file("test", "fips-and-base.cnf");
    my $defaultconf = srctop_file("test", "default.cnf");

    $ENV{OPENSSL_CONF} = $fipsconf;

    ok(run(app(['openssl', 'ecparam',
                '-in', data_file('valid', 'secp384r1-explicit.pem'),
                '-check'])),
       "Loading explicitly encoded valid curve");

    ok(run(app(['openssl', 'ecparam',
                '-in', data_file('valid', 'secp384r1-named.pem'),
                '-check'])),
       "Loading named valid curve");

    ok(!run(app(['openssl', 'ecparam',
                '-in', data_file('valid', 'secp112r1-named.pem'),
                '-check'])),
       "Fail loading named non-fips curve");

    ok(!run(app(['openssl', 'pkeyparam',
                '-in', data_file('valid', 'secp112r1-named.pem'),
                '-check'])),
       "Fail loading named non-fips curve using pkeyparam");

    ok(run(app(['openssl', 'ecparam',
                '-provider', 'default',
                '-propquery', '?fips!=yes',
                '-in', data_file('valid', 'secp112r1-named.pem'),
                '-check'])),
       "Loading named non-fips curve in FIPS mode with non-FIPS property".
       " query");

    ok(run(app(['openssl', 'pkeyparam',
                '-provider', 'default',
                '-propquery', '?fips!=yes',
                '-in', data_file('valid', 'secp112r1-named.pem'),
                '-check'])),
       "Loading named non-fips curve in FIPS mode with non-FIPS property".
       " query using pkeyparam");

    ok(!run(app(['openssl', 'ecparam',
                '-genkey', '-name', 'secp112r1'])),
       "Fail generating key for named non-fips curve");

    ok(run(app(['openssl', 'ecparam',
                '-provider', 'default',
                '-propquery', '?fips!=yes',
                '-genkey', '-name', 'secp112r1'])),
       "Generating key for named non-fips curve with non-FIPS property query");

    $ENV{OPENSSL_CONF} = $defaultconf;
};
