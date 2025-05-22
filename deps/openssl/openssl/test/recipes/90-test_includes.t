#! /usr/bin/env perl

use strict;
use warnings;
use OpenSSL::Test qw/:DEFAULT bldtop_dir data_file/;
use OpenSSL::Test::Utils;
use Cwd qw(abs_path);

setup("test_includes");

plan skip_all => "test_includes doesn't work without posix-io"
    if disabled("posix-io");

delete $ENV{OPENSSL_CONF_INCLUDE};

plan tests =>                   # The number of tests being performed
    7
    + ($^O eq "VMS" ? 2 : 0);

$ENV{OPENSSL_MODULES} = abs_path(bldtop_dir("providers"));

ok(run(test(["conf_include_test", data_file("includes.cnf")])), "test directory includes");
ok(run(test(["conf_include_test", data_file("includes-file.cnf")])), "test file includes");
ok(run(test(["conf_include_test", data_file("includes-eq.cnf")])), "test includes with equal character");
ok(run(test(["conf_include_test", data_file("includes-eq-ws.cnf")])), "test includes with equal and whitespaces");
if ($^O eq "VMS") {
    ok(run(test(["conf_include_test", data_file("vms-includes.cnf")])),
       "test directory includes, VMS syntax");
    ok(run(test(["conf_include_test", data_file("vms-includes-file.cnf")])),
       "test file includes, VMS syntax");
}
ok(run(test(["conf_include_test", "-f", data_file("includes-broken.cnf")])), "test broken includes");
ok(run(test(["conf_include_test",  "-f", data_file("incdir.cnf")])), "test includedir");

SKIP: {
    skip "Skipping legacy test", 1
        if disabled("legacy");
    ok(run(test(["conf_include_test", "-providers", data_file("includes-prov-dir.cnf")])),
       "test directory includes with provider configs");
}
