#! /usr/bin/env perl
# Copyright 2016-2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_file srctop_dir bldtop_dir bldtop_file result_dir result_file/;
use File::Temp qw(tempfile);

BEGIN {
setup("test_sslapi");
}

my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);
my $fipsmodcfg_filename = "fipsmodule.cnf";
my $fipsmodcfg = bldtop_file("test", $fipsmodcfg_filename);

my $provconf = srctop_file("test", "fips-and-base.cnf");

# A modified copy of "fipsmodule.cnf"
my $fipsmodcfgnew_filename = "fipsmodule_mod.cnf";
my $fipsmodcfgnew = result_file($fipsmodcfgnew_filename);

# An interum modified copy of "fipsmodule.cnf"
my $fipsmodcfgtmp_filename = "fipsmodule_tmp.cnf";
my $fipsmodcfgtmp = result_file($fipsmodcfgtmp_filename);

# A modified copy of "fips-and-base.cnf"
my $provconfnew = result_file("fips-and-base-temp.cnf");

plan skip_all => "No TLS/SSL protocols are supported by this OpenSSL build"
    if alldisabled(grep { $_ ne "ssl3" } available_protocols("tls"));

plan tests => 4;

(undef, my $tmpfilename) = tempfile();

ok(run(test(["sslapitest", srctop_dir("test", "certs"),
             srctop_file("test", "recipes", "90-test_sslapi_data",
                         "passwd.txt"), $tmpfilename, "default",
             srctop_file("test", "default.cnf"),
             srctop_file("test",
                         "recipes",
                         "90-test_sslapi_data",
                         "dhparams.pem")])),
             "running sslapitest");

SKIP: {
    skip "Skipping FIPS tests", 2
        if $no_fips;

    # NOTE that because by default we setup fips provider in pedantic mode,
    # with >= 3.1.0 this just runs test_no_ems() to check that the connection
    # fails if ems is not used and the fips check is enabled.
    ok(run(test(["sslapitest", srctop_dir("test", "certs"),
                 srctop_file("test", "recipes", "90-test_sslapi_data",
                             "passwd.txt"), $tmpfilename, "fips",
                 $provconf,
                 srctop_file("test",
                             "recipes",
                             "90-test_sslapi_data",
                             "dhparams.pem")])),
                 "running sslapitest with default fips config");

    run(test(["fips_version_test", "-config", $provconf, ">=3.1.0"]),
             capture => 1, statusvar => \my $exit);

    skip "FIPS provider version is too old for TLS_PRF EMS option test", 1
        if !$exit;

    # Read in a text $infile and replace the regular expression in $srch with the
    # value in $repl and output to a new file $outfile.
    sub replace_line_file_internal {

        my ($infile, $srch, $repl, $outfile) = @_;
        my $msg;

        open(my $in, "<", $infile) or return 0;
        read($in, $msg, 1024);
        close $in;

        $msg =~ s/$srch/$repl/;

        open(my $fh, ">", $outfile) or return 0;
        print $fh $msg;
        close $fh;
        return 1;
    }

    # Read in the text input file $infile
    # and replace a single Key = Value line with a new value in $value.
    # OR remove the Key = Value line if the passed in $value is empty.
    # and then output a new file $outfile.
    # $key is the Key to find
    sub replace_kv_file {
        my ($infile, $key, $value, $outfile) = @_;
        my $srch = qr/$key\s*=\s*\S*\n/;
        my $rep;
        if ($value eq "") {
            $rep = "";
        } else {
           $rep = "$key = $value\n";
        }
        return replace_line_file_internal($infile, $srch, $rep, $outfile);
    }

    # Read in the text $input file
    # and search for the $key and replace with $newkey
    # and then output a new file $outfile.
    sub replace_line_file {
        my ($infile, $key, $newkey, $outfile) = @_;
        my $srch = qr/$key/;
        my $rep = "$newkey";
        return replace_line_file_internal($infile,
                                          $srch, $rep, $outfile);
    }

    # The default fipsmodule.cnf in tests is set with -pedantic.
    # In order to enable the tls1-prf-ems-check=0 in a fips config file
    # copy the existing fipsmodule.cnf and modify it.
    # Then copy fips-and-base.cfg to make a file that includes the changed file
    $ENV{OPENSSL_CONF_INCLUDE} = result_dir();
    ok(replace_kv_file($fipsmodcfg,
                       'tls1-prf-ems-check', '0',
                       $fipsmodcfgtmp)
       && replace_kv_file($fipsmodcfgtmp,
                          'rsa-pkcs15-pad-disabled', '0',
                          $fipsmodcfgnew)
       && replace_line_file($provconf,
                            $fipsmodcfg_filename, $fipsmodcfgnew_filename,
                            $provconfnew)
       && run(test(["sslapitest", srctop_dir("test", "certs"),
                    srctop_file("test", "recipes", "90-test_sslapi_data",
                                "passwd.txt"),
                    $tmpfilename, "fips",
                    $provconfnew,
                    srctop_file("test",
                                "recipes",
                                "90-test_sslapi_data",
                                "dhparams.pem")])),
       "running sslapitest with modified fips config");
}

ok(run(test(["ssl_handshake_rtt_test"])),"running ssl_handshake_rtt_test");

unlink $tmpfilename;
