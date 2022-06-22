#! /usr/bin/env perl
# Copyright 2015-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Basename;
use File::Compare qw/compare_text/;
use OpenSSL::Glob;
use OpenSSL::Test qw/:DEFAULT srctop_dir srctop_file/;
use OpenSSL::Test::Utils qw/disabled alldisabled available_protocols/;

setup("test_ssl_new");

$ENV{TEST_CERTS_DIR} = srctop_dir("test", "certs");
$ENV{CTLOG_FILE} = srctop_file("test", "ct", "log_list.conf");

my @conf_srcs =  glob(srctop_file("test", "ssl-tests", "*.conf.in"));
map { s/;.*// } @conf_srcs if $^O eq "VMS";
my @conf_files = map { basename($_, ".in") } @conf_srcs;
map { s/\^// } @conf_files if $^O eq "VMS";

# We hard-code the number of tests to double-check that the globbing above
# finds all files as expected.
plan tests => 30;  # = scalar @conf_srcs

# Some test results depend on the configuration of enabled protocols. We only
# verify generated sources in the default configuration.
my $is_default_tls = (disabled("ssl3") && !disabled("tls1") &&
                      !disabled("tls1_1") && !disabled("tls1_2") &&
                      !disabled("tls1_3"));

my $is_default_dtls = (!disabled("dtls1") && !disabled("dtls1_2"));

my @all_pre_tls1_3 = ("ssl3", "tls1", "tls1_1", "tls1_2");
my $no_tls = alldisabled(available_protocols("tls"));
my $no_tls_below1_3 = $no_tls || (disabled("tls1_2") && !disabled("tls1_3"));
my $no_pre_tls1_3 = alldisabled(@all_pre_tls1_3);
my $no_dtls = alldisabled(available_protocols("dtls"));
my $no_npn = disabled("nextprotoneg");
my $no_ct = disabled("ct");
my $no_ec = disabled("ec");
my $no_dh = disabled("dh");
my $no_dsa = disabled("dsa");
my $no_ec2m = disabled("ec2m");
my $no_ocsp = disabled("ocsp");

# Add your test here if the test conf.in generates test cases and/or
# expectations dynamically based on the OpenSSL compile-time config.
my %conf_dependent_tests = (
  "02-protocol-version.conf" => !$is_default_tls,
  "04-client_auth.conf" => !$is_default_tls || !$is_default_dtls
                           || !disabled("sctp"),
  "05-sni.conf" => disabled("tls1_1"),
  "07-dtls-protocol-version.conf" => !$is_default_dtls || !disabled("sctp"),
  "10-resumption.conf" => !$is_default_tls,
  "11-dtls_resumption.conf" => !$is_default_dtls || !disabled("sctp"),
  "16-dtls-certstatus.conf" => !$is_default_dtls || !disabled("sctp"),
  "17-renegotiate.conf" => disabled("tls1_2"),
  "18-dtls-renegotiate.conf" => disabled("dtls1_2") || !disabled("sctp"),
  "19-mac-then-encrypt.conf" => !$is_default_tls,
  "20-cert-select.conf" => !$is_default_tls || $no_dh || $no_dsa,
  "22-compression.conf" => !$is_default_tls,
  "25-cipher.conf" => disabled("poly1305") || disabled("chacha"),
  "27-ticket-appdata.conf" => !$is_default_tls,
  "28-seclevel.conf" => disabled("tls1_2") || $no_ec,
  "30-supported-groups.conf" => disabled("tls1_2") || disabled("tls1_3")
                                || $no_ec || $no_ec2m
);

# Add your test here if it should be skipped for some compile-time
# configurations. Default is $no_tls but some tests have different skip
# conditions.
my %skip = (
  "06-sni-ticket.conf" => $no_tls_below1_3,
  "07-dtls-protocol-version.conf" => $no_dtls,
  "08-npn.conf" => (disabled("tls1") && disabled("tls1_1")
                    && disabled("tls1_2")) || $no_npn,
  "10-resumption.conf" => disabled("tls1_1") || disabled("tls1_2"),
  "11-dtls_resumption.conf" => disabled("dtls1") || disabled("dtls1_2"),
  "12-ct.conf" => $no_tls || $no_ct || $no_ec,
  # We could run some of these tests without TLS 1.2 if we had a per-test
  # disable instruction but that's a bizarre configuration not worth
  # special-casing for.
  # TODO(TLS 1.3): We should review this once we have TLS 1.3.
  "13-fragmentation.conf" => disabled("tls1_2"),
  "14-curves.conf" => disabled("tls1_2") || $no_ec || $no_ec2m,
  "15-certstatus.conf" => $no_tls || $no_ocsp,
  "16-dtls-certstatus.conf" => $no_dtls || $no_ocsp,
  "17-renegotiate.conf" => $no_tls_below1_3,
  "18-dtls-renegotiate.conf" => $no_dtls,
  "19-mac-then-encrypt.conf" => $no_pre_tls1_3,
  "20-cert-select.conf" => disabled("tls1_2") || $no_ec,
  "21-key-update.conf" => disabled("tls1_3"),
  "22-compression.conf" => disabled("zlib") || $no_tls,
  "23-srp.conf" => (disabled("tls1") && disabled ("tls1_1")
                    && disabled("tls1_2")) || disabled("srp"),
  "24-padding.conf" => disabled("tls1_3"),
  "25-cipher.conf" => disabled("ec") || disabled("tls1_2"),
  "26-tls13_client_auth.conf" => disabled("tls1_3"),
  "29-dtls-sctp-label-bug.conf" => disabled("sctp") || disabled("sock"),
);

foreach my $conf (@conf_files) {
    subtest "Test configuration $conf" => sub {
        test_conf($conf,
                  $conf_dependent_tests{$conf} || $^O eq "VMS" ?  0 : 1,
                  defined($skip{$conf}) ? $skip{$conf} : $no_tls);
    }
}

sub test_conf {
    plan tests => 3;

    my ($conf, $check_source, $skip) = @_;

    my $conf_file = srctop_file("test", "ssl-tests", $conf);
    my $tmp_file = "${conf}.$$.tmp";
    my $run_test = 1;

  SKIP: {
      # "Test" 1. Generate the source.
      my $input_file = $conf_file . ".in";

      skip 'failure', 2 unless
        ok(run(perltest(["generate_ssl_tests.pl", $input_file],
                        interpreter_args => [ "-I", srctop_dir("util", "perl")],
                        stdout => $tmp_file)),
           "Getting output from generate_ssl_tests.pl.");

    SKIP: {
        # Test 2. Compare against existing output in test/ssl_tests.conf.
        skip "Skipping generated source test for $conf", 1
          if !$check_source;

        $run_test = is(cmp_text($tmp_file, $conf_file), 0,
                       "Comparing generated sources.");
      }

      # Test 3. Run the test.
      skip "No tests available; skipping tests", 1 if $skip;
      skip "Stale sources; skipping tests", 1 if !$run_test;

      ok(run(test(["ssl_test", $tmp_file])), "running ssl_test $conf");
    }

    unlink glob $tmp_file;
}

sub cmp_text {
    return compare_text(@_, sub {
        $_[0] =~ s/\R//g;
        $_[1] =~ s/\R//g;
        return $_[0] ne $_[1];
    });
}
