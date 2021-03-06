#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use POSIX;
use File::Spec::Functions qw/splitdir curdir catfile/;
use File::Compare;
use OpenSSL::Test qw/:DEFAULT cmdstr srctop_file/;
use OpenSSL::Test::Utils;

setup("test_tsa");

plan skip_all => "TS is not supported by this OpenSSL build"
    if disabled("ts");

# All these are modified inside indir further down. They need to exist
# here, however, to be available in all subroutines.
my $openssl_conf;
my $testtsa;
my $CAtsa;
my @RUN;

sub create_tsa_cert {
    my $INDEX = shift;
    my $EXT = shift;
    my $r = 1;
    $ENV{TSDNSECT} = "ts_cert_dn";

    ok(run(app(["openssl", "req", "-config", $openssl_conf, "-new",
                "-out", "tsa_req${INDEX}.pem",
                "-keyout", "tsa_key${INDEX}.pem"])));
    note "using extension $EXT";
    ok(run(app(["openssl", "x509", "-req",
                "-in", "tsa_req${INDEX}.pem",
                "-out", "tsa_cert${INDEX}.pem",
                "-CA", "tsaca.pem", "-CAkey", "tsacakey.pem",
                "-CAcreateserial",
                "-extfile", $openssl_conf, "-extensions", $EXT])));
}

sub create_time_stamp_response {
    my $queryfile = shift;
    my $outputfile = shift;
    my $datafile = shift;

    ok(run(app([@RUN, "-reply", "-section", "$datafile",
                "-queryfile", "$queryfile", "-out", "$outputfile"])));
}

sub verify_time_stamp_response {
    my $queryfile = shift;
    my $inputfile = shift;
    my $datafile = shift;

    ok(run(app([@RUN, "-verify", "-queryfile", "$queryfile",
                "-in", "$inputfile", "-CAfile", "tsaca.pem",
                "-untrusted", "tsa_cert1.pem"])));
    ok(run(app([@RUN, "-verify", "-data", "$datafile",
                "-in", "$inputfile", "-CAfile", "tsaca.pem",
                "-untrusted", "tsa_cert1.pem"])));
}

sub verify_time_stamp_response_fail {
    my $queryfile = shift;
    my $inputfile = shift;

    ok(!run(app([@RUN, "-verify", "-queryfile", "$queryfile",
                 "-in", "$inputfile", "-CAfile", "tsaca.pem",
                 "-untrusted", "tsa_cert1.pem"])));
}

# main functions

plan tests => 20;

note "setting up TSA test directory";
indir "tsa" => sub
{
    $openssl_conf = srctop_file("test", "CAtsa.cnf");
    $testtsa = srctop_file("test", "recipes", "80-test_tsa.t");
    $CAtsa = srctop_file("test", "CAtsa.cnf");
    @RUN = ("openssl", "ts", "-config", $openssl_conf);

    # ../apps/CA.pl needs these
    $ENV{OPENSSL_CONFIG} = "-config $openssl_conf";
    $ENV{OPENSSL} = cmdstr(app(["openssl"]), display => 1);

 SKIP: {
     $ENV{TSDNSECT} = "ts_ca_dn";
     skip "failed", 19
         unless ok(run(app(["openssl", "req", "-config", $openssl_conf,
                            "-new", "-x509", "-nodes",
                            "-out", "tsaca.pem", "-keyout", "tsacakey.pem"])),
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
         unless ok(run(app([@RUN, "-query", "-data", $testtsa,
                            "-tspolicy", "tsa_policy1", "-cert",
                            "-out", "req1.tsq"])),
                   'creating req1.req time stamp request for file testtsa');

     ok(run(app([@RUN, "-query", "-in", "req1.tsq", "-text"])),
        'printing req1.req');

     subtest 'generating valid response for req1.req' => sub {
         create_time_stamp_response("req1.tsq", "resp1.tsr", "tsa_config1")
     };

     ok(run(app([@RUN, "-reply", "-in", "resp1.tsr", "-text"])),
        'printing response');

     subtest 'verifying valid response' => sub {
         verify_time_stamp_response("req1.tsq", "resp1.tsr", $testtsa)
     };

     skip "failed", 11
         unless subtest 'verifying valid token' => sub {
             ok(run(app([@RUN, "-reply", "-in", "resp1.tsr",
                         "-out", "resp1.tsr.token", "-token_out"])));
             ok(run(app([@RUN, "-verify", "-queryfile", "req1.tsq",
                         "-in", "resp1.tsr.token", "-token_in",
                         "-CAfile", "tsaca.pem",
                         "-untrusted", "tsa_cert1.pem"])));
             ok(run(app([@RUN, "-verify", "-data", $testtsa,
                         "-in", "resp1.tsr.token", "-token_in",
                         "-CAfile", "tsaca.pem",
                         "-untrusted", "tsa_cert1.pem"])));
     };

     skip "failed", 10
         unless ok(run(app([@RUN, "-query", "-data", $testtsa,
                            "-tspolicy", "tsa_policy2", "-no_nonce",
                            "-out", "req2.tsq"])),
                   'creating req2.req time stamp request for file testtsa');

     ok(run(app([@RUN, "-query", "-in", "req2.tsq", "-text"])),
        'printing req2.req');

     skip "failed", 8
         unless subtest 'generating valid response for req2.req' => sub {
             create_time_stamp_response("req2.tsq", "resp2.tsr", "tsa_config1")
     };

     skip "failed", 7
         unless subtest 'checking -token_in and -token_out options with -reply' => sub {
             my $RESPONSE2="resp2.tsr.copy.tsr";
             my $TOKEN_DER="resp2.tsr.token.der";

             ok(run(app([@RUN, "-reply", "-in", "resp2.tsr",
                         "-out", "$TOKEN_DER", "-token_out"])));
             ok(run(app([@RUN, "-reply", "-in", "$TOKEN_DER",
                         "-token_in", "-out", "$RESPONSE2"])));
             is(compare($RESPONSE2, "resp2.tsr"), 0);
             ok(run(app([@RUN, "-reply", "-in", "resp2.tsr",
                         "-text", "-token_out"])));
             ok(run(app([@RUN, "-reply", "-in", "$TOKEN_DER",
                         "-token_in", "-text", "-token_out"])));
             ok(run(app([@RUN, "-reply", "-queryfile", "req2.tsq",
                         "-text", "-token_out"])));
     };

     ok(run(app([@RUN, "-reply", "-in", "resp2.tsr", "-text"])),
        'printing response');

     subtest 'verifying valid response' => sub {
         verify_time_stamp_response("req2.tsq", "resp2.tsr", $testtsa)
     };

     subtest 'verifying response against wrong request, it should fail' => sub {
         verify_time_stamp_response_fail("req1.tsq", "resp2.tsr")
     };

     subtest 'verifying response against wrong request, it should fail' => sub {
         verify_time_stamp_response_fail("req2.tsq", "resp1.tsr")
     };

     skip "failure", 2
         unless ok(run(app([@RUN, "-query", "-data", $CAtsa,
                            "-no_nonce", "-out", "req3.tsq"])),
                   "creating req3.req time stamp request for file CAtsa.cnf");

     ok(run(app([@RUN, "-query", "-in", "req3.tsq", "-text"])),
        'printing req3.req');

     subtest 'verifying response against wrong request, it should fail' => sub {
         verify_time_stamp_response_fail("req3.tsq", "resp1.tsr")
     };
    }
}, create => 1, cleanup => 1
