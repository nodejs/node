#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use POSIX;
use File::Spec::Functions qw/splitdir curdir catfile/;
use File::Compare;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file data_file/;
use OpenSSL::Test::Utils;

setup("test_tsa");

plan skip_all => "TS is not supported by this OpenSSL build"
    if disabled("ts");

# All these are modified inside indir further down. They need to exist
# here, however, to be available in all subroutines.
my $openssl_conf;
my $testtsa;
my $tsacakey;
my $CAtsa;
my @QUERY = ("openssl", "ts", "-query");
my @REPLY;
my @VERIFY = ("openssl", "ts", "-verify");

sub create_tsa_cert {
    my $INDEX = shift;
    my $EXT = shift;
    my $r = 1;
    $ENV{TSDNSECT} = "ts_cert_dn";

    ok(run(app(["openssl", "req", "-config", $openssl_conf, "-new",
                "-out", "tsa_req${INDEX}.pem",
                "-key", srctop_file("test", "certs", "alt${INDEX}-key.pem"),
                "-keyout", "tsa_key${INDEX}.pem"])));
    note "using extension $EXT";
    ok(run(app(["openssl", "x509", "-req",
                "-in", "tsa_req${INDEX}.pem",
                "-out", "tsa_cert${INDEX}.pem",
                "-CA", "tsaca.pem", "-CAkey", $tsacakey,
                "-CAcreateserial",
                "-extfile", $openssl_conf, "-extensions", $EXT])));
}

sub create_resp {
    my $config = shift;
    my $chain = shift;
    my $queryfile = shift;
    my $outputfile = shift;

    ok(run(app([@REPLY, "-section", $config, "-queryfile", $queryfile,
                "-chain", $chain, # this overrides "certs" entry in config
                "-out", $outputfile])));
}

sub verify_ok {
    my $datafile = shift;
    my $queryfile = shift;
    my $inputfile = shift;
    my $untrustedfile = shift;

    ok(run(app([@VERIFY, "-queryfile", $queryfile, "-in", $inputfile,
                "-CAfile", "tsaca.pem", "-untrusted", $untrustedfile])));
    ok(run(app([@VERIFY, "-data", $datafile, "-in", $inputfile,
                "-CAfile", "tsaca.pem", "-untrusted", $untrustedfile])));
}

sub verify_fail {
    my $queryfile = shift;
    my $inputfile = shift;
    my $untrustedfile = shift; # is needed for resp2, but not for resp1
    my $cafile = shift;

    ok(!run(app([@VERIFY, "-queryfile", $queryfile, "-in", $inputfile,
                 "-untrusted", $untrustedfile, "-CAfile", $cafile])));
}

# main functions

plan tests => 27;

note "setting up TSA test directory";
indir "tsa" => sub
{
    $openssl_conf = srctop_file("test", "CAtsa.cnf");
    $testtsa = srctop_file("test", "recipes", "80-test_tsa.t");
    $tsacakey = srctop_file("test", "certs", "ca-key.pem");
    $CAtsa = srctop_file("test", "CAtsa.cnf");
    @REPLY = ("openssl", "ts", "-config", $openssl_conf, "-reply");

    # ../apps/CA.pl needs these
    $ENV{OPENSSL_CONFIG} = "-config $openssl_conf";
    $ENV{OPENSSL} = cmdstr(app(["openssl"]), display => 1);

 SKIP: {
     $ENV{TSDNSECT} = "ts_ca_dn";
     skip "failed", 19
         unless ok(run(app(["openssl", "req", "-config", $openssl_conf,
                            "-new", "-x509", "-noenc",
                            "-out", "tsaca.pem", "-key", $tsacakey])),
                   'creating a new CA for the TSA tests');

     skip "failed", 18
         unless subtest 'creating tsa_cert1.pem TSA server cert' => sub {
             create_tsa_cert("1", "tsa_cert")
     };

     skip "failed", 17
         unless subtest 'creating tsa_cert2.pem non-TSA server cert' => sub {
             create_tsa_cert("2", "non_tsa_cert")
     };

     skip "failed", 16
         unless ok(run(app([@QUERY, "-data", $testtsa,
                            "-tspolicy", "tsa_policy1", "-cert",
                            "-out", "req1.tsq"])),
                   'creating req1.req time stamp request for file testtsa');

     ok(run(app([@QUERY, "-in", "req1.tsq", "-text"])),
        'printing req1.req');

     subtest 'generating valid response for req1.req' => sub {
         create_resp("tsa_config1", "tsaca.pem", "req1.tsq", "resp1.tsr")
     };

     subtest 'generating response with wrong 2nd certid for req1.req' => sub {
         create_resp("tsa_config1", "tsa_cert1.pem", "req1.tsq",
                     "resp1_invalid.tsr")
     };

     ok(run(app([@REPLY, "-in", "resp1.tsr", "-text"])),
        'printing response');

     subtest 'verifying valid response' => sub {
         verify_ok($testtsa, "req1.tsq", "resp1.tsr", "tsa_cert1.pem")
     };

     skip "failed", 11
         unless subtest 'verifying valid token' => sub {
             ok(run(app([@REPLY, "-in", "resp1.tsr",
                         "-out", "resp1.tsr.token", "-token_out"])));
             ok(run(app([@VERIFY, "-queryfile", "req1.tsq",
                         "-in", "resp1.tsr.token", "-token_in",
                         "-CAfile", "tsaca.pem"])));
             ok(run(app([@VERIFY, "-data", $testtsa,
                         "-in", "resp1.tsr.token", "-token_in",
                         "-CAfile", "tsaca.pem"])));
     };

     skip "failed", 10
         unless ok(run(app([@QUERY, "-data", $testtsa,
                            "-tspolicy", "tsa_policy2", "-no_nonce",
                            "-out", "req2.tsq"])),
                   'creating req2.req time stamp request for file testtsa');

     ok(run(app([@QUERY, "-in", "req2.tsq", "-text"])),
        'printing req2.req');

     skip "failed", 8
         unless subtest 'generating valid response for req2.req' => sub {
             create_resp("tsa_config1", "tsaca.pem", "req2.tsq", "resp2.tsr")
     };

     skip "failed", 7
         unless subtest 'checking -token_in and -token_out options with -reply' => sub {
             my $RESPONSE2="resp2.tsr.copy.tsr";
             my $TOKEN_DER="resp2.tsr.token.der";

             ok(run(app([@REPLY, "-in", "resp2.tsr",
                         "-out", "$TOKEN_DER", "-token_out"])));
             ok(run(app([@REPLY, "-in", "$TOKEN_DER",
                         "-token_in", "-out", "$RESPONSE2"])));
             is(compare($RESPONSE2, "resp2.tsr"), 0);
             ok(run(app([@REPLY, "-in", "resp2.tsr",
                         "-text", "-token_out"])));
             ok(run(app([@REPLY, "-in", "$TOKEN_DER",
                         "-token_in", "-text", "-token_out"])));
             ok(run(app([@REPLY, "-queryfile", "req2.tsq",
                         "-text", "-token_out"])));
     };

     ok(run(app([@REPLY, "-in", "resp2.tsr", "-text"])),
        'printing response');

     subtest 'verifying valid resp1, wrong untrusted is not used' => sub {
         verify_ok($testtsa, "req1.tsq", "resp1.tsr", "tsa_cert2.pem")
     };

     subtest 'verifying invalid resp1 with wrong 2nd certid' => sub {
         verify_fail($testtsa, "req1.tsq", "resp1_invalid.tsr", "tsa_cert2.pem")
     };

     subtest 'verifying valid resp2, correct untrusted being used' => sub {
         verify_ok($testtsa, "req2.tsq", "resp2.tsr", "tsa_cert1.pem")
     };

     subtest 'verifying resp2 against wrong req1 should fail' => sub {
         verify_fail("req1.tsq", "resp2.tsr", "tsa_cert1.pem", "tsaca.pem")
     };

     subtest 'verifying resp1 against wrong req2 should fail' => sub {
         verify_fail("req2.tsq", "resp1.tsr", "tsa_cert1.pem", "tsaca.pem")
     };

     subtest 'verifying resp1 using wrong untrusted should fail' => sub {
         verify_fail("req2.tsq", "resp2.tsr", "tsa_cert2.pem", "tsaca.pem")
     };

     subtest 'verifying resp1 using wrong root should fail' => sub {
         verify_fail("req1.tsq", "resp1.tsr", "tsa_cert1.pem", "tsa_cert1.pem")
     };

     skip "failure", 2
         unless ok(run(app([@QUERY, "-data", $CAtsa,
                            "-no_nonce", "-out", "req3.tsq"])),
                   "creating req3.req time stamp request for file CAtsa.cnf");

     ok(run(app([@QUERY, "-in", "req3.tsq", "-text"])),
        'printing req3.req');

     subtest 'verifying resp1 against wrong req3 should fail' => sub {
         verify_fail("req3.tsq", "resp1.tsr", "tsa_cert1.pem", "tsaca.pem")
     };
    }

    # verifying response with two ESSCertIDs, referring to leaf cert
    # "sectigo-signer.pem" and intermediate cert "sectigo-time-stamping-ca.pem"
    # 1. validation chain contains these certs and root "user-trust-ca.pem"
    ok(run(app([@VERIFY, "-no_check_time",
                "-queryfile", data_file("all-zero.tsq"),
                "-in", data_file("sectigo-all-zero.tsr"),
                "-CAfile", data_file("user-trust-ca.pem")])),
     "validation with two ESSCertIDs and 3-element chain");
    # 2. validation chain contains these certs, a cross-cert, and different root
    ok(run(app([@VERIFY, "-no_check_time",
                "-queryfile", data_file("all-zero.tsq"),
                "-in", data_file("sectigo-all-zero.tsr"),
                "-untrusted", data_file("user-trust-ca-aaa.pem"),
                "-CAfile", data_file("comodo-aaa.pem")])),
     "validation with two ESSCertIDs and 4-element chain");

}, create => 1, cleanup => 1
