#! /usr/bin/env perl
# Copyright 2007-2021 The OpenSSL Project Authors. All Rights Reserved.
# Copyright Nokia 2007-2019
# Copyright Siemens AG 2015-2019
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use POSIX;
use File::Compare qw/compare_text/;
use OpenSSL::Test qw/:DEFAULT with srctop_file srctop_dir bldtop_dir result_file/;
use OpenSSL::Test::Utils;

BEGIN {
    setup("test_cmp_cli");
}
use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

plan skip_all => "These tests are not supported in a fuzz build"
    if config('options') =~ /-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION/;

plan skip_all => "These tests are not supported in a no-cmp build"
    if disabled("cmp");

# Prevent MSys2 filename munging for arguments that look like file paths but
# aren't
$ENV{MSYS2_ARG_CONV_EXCL} = "/CN=";

my @app = qw(openssl cmp);

my @cmp_basic_tests = (
    [ "show help",                        [ "-help"               ], 1 ],
    [ "CLI option not starting with '-'", [  "days", "1"          ], 0 ],
    [ "unknown CLI option",               [ "-dayss"              ], 0 ],
    [ "bad int syntax: non-digit",        [ "-days", "a/"         ], 0 ],
    [ "bad int syntax: float",            [ "-days", "3.14"       ], 0 ],
    [ "bad int syntax: trailing garbage", [ "-days", "314_+"      ], 0 ],
    [ "bad int: out of range",            [ "-days", "2147483648" ], 0 ],
    );

my @cmp_server_tests = (
    [ "with polling",             [ "-poll_count", "1"       ], 1 ]
    );

# loader_attic doesn't build on VMS, so we don't test it
push @cmp_server_tests, (
    [ "with loader_attic engine", [ "-engine", "loader_attic"], 1 ]
    )
    unless disabled('loadereng');

plan tests => @cmp_basic_tests + @cmp_server_tests;

foreach (@cmp_basic_tests) {
    my $title = $$_[0];
    my $params = $$_[1];
    my $expected = $$_[2];
    ok($expected == run(app([@app, "-config", '', @$params])),
       $title);
}

# these use the mock server directly in the cmp app, without TCP
foreach (@cmp_server_tests) {
    my $title = $$_[0];
    my $extra_args = $$_[1];
    my $expected = $$_[2];
    my $secret = "pass:test";
    my $rsp_cert = srctop_file('test',  'certs', 'ee-cert-1024.pem');
    my $outfile = result_file("test.certout.pem");
    ok($expected ==
       run(app([@app, "-config", '', @$extra_args,
                "-use_mock_srv", "-srv_ref", "mock server",
                "-srv_secret", $secret,
                "-rsp_cert", $rsp_cert,
                "-cmd", "cr",
                "-subject", "/CN=any",
                "-newkey", srctop_file('test', 'certs', 'ee-key-1024.pem'),
                "-secret", $secret,
                "-ref", "client under test",
                "-certout", $outfile]))
       && compare_text($outfile, $rsp_cert) == 0,
       $title);
    # not unlinking $outfile
}
