#! /usr/bin/env perl
# Copyright 2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test;
use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT bldtop_file data_file srctop_file cmdstr/;

setup("test_windows_registry");

plan skip_all => "Windows registry tests are only available on windows"
    if $^O !~ /(MSWin)/;

my $actual;
my $expect;

my @tempout = run(app(["openssl", "version", "-w"]), capture => 1);
my $context = "@tempout";
$context =~ s/^.*: //;


@tempout = run(app(["openssl", "version", "-v"]), capture => 1);
my $version = "@tempout";
$version =~ s/^OpenSSL //;
$version =~ s/(^[0-9]+\.[0-9]+)(.*$)/\1/;

my $regkey = "HKLM\\SOFTWARE\\OpenSSL-".$version."-".$context;
$regkey =~ s/\n//g;
print "REGKEY IS $regkey\n";

my $exit = run(cmd(["reg.exe", "query", $regkey, "/v", "OPENSSLDIR", "/reg:32"]));

plan skip_all => "Skipping test as registry keys aren't set"
    if $exit == 0;

plan tests => 3;

my @expectossldir = run(cmd(["reg.exe", "query", $regkey, "/reg:32", "/t", "REG_EXPAND_SZ", "/v", "OPENSSLDIR"]), capture => 1);

my @expectengdir = run(cmd(["reg.exe", "query", $regkey, "/reg:32", "/t", "REG_EXPAND_SZ", "/v", "ENGINESDIR"]), capture => 1);

my @expectmoddir = run(cmd(["reg.exe", "query", $regkey, "/reg:32", "/t", "REG_EXPAND_SZ", "/v", "MODULESDIR"]), capture => 1);

my @ossldir = run(app(["openssl", "version", "-d"]), capture => 1);

print "@ossldir";
$expect = "@expectossldir";
$actual = "@ossldir";
$expect =~ s/HKEY_LOCAL_MACHINE.*\n*//;
$expect =~ s/\n//g;
$expect =~ s/.*REG_EXPAND_SZ *//;
$expect =~ s/ .*$//;
$actual =~ s/OPENSSLDIR: *//;

ok(grep(/$expect/,$actual), "Confirming version output for openssldir from registry");

my @osslengineout = run(app(["openssl", "version", "-e"]), capture => 1);

$expect = "@expectengdir";
$actual = "@osslengineout";
$expect =~ s/HKEY_LOCAL_MACHINE.*\n*//;
$expect =~ s/\n//g;
$expect =~ s/.*REG_EXPAND_SZ *//;
$expect =~ s/ .*$//;
$actual =~ s/ENGINESDIR: *//;

ok(grep(/$expect/, $actual) == 1, "Confirming version output for enginesdir from registry");

my @osslmoduleout = run(app(["openssl", "version", "-m"]), capture => 1);

$expect = "@expectmoddir";
$actual = "@osslmoduleout";
$expect =~ s/HKEY_LOCAL_MACHINE.*\n*//;
$expect =~ s/\n//g;
$expect =~ s/.*REG_EXPAND_SZ *//;
$expect =~ s/ .*$//;
$actual =~ s/MODULESSDIR: *//;
ok(grep(/$expect/, $actual) == 1, "Confirming version output for modulesdir from registry");
