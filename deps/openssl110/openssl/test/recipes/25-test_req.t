#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_req");

plan tests => 4;

require_ok(srctop_file('test','recipes','tconversion.pl'));

open RND, ">>", ".rnd";
print RND "string to make the random number generator think it has entropy";
close RND;
subtest "generating certificate requests" => sub {
    my @req_new;
    if (disabled("rsa")) {
	@req_new = ("-newkey", "dsa:".srctop_file("apps", "dsa512.pem"));
    } else {
	@req_new = ("-new");
	note("There should be a 2 sequences of .'s and some +'s.");
	note("There should not be more that at most 80 per line");
    }

    plan tests => 2;

    ok(run(app(["openssl", "req", "-config", srctop_file("test", "test.cnf"),
		@req_new, "-out", "testreq.pem"])),
       "Generating request");

    ok(run(app(["openssl", "req", "-config", srctop_file("test", "test.cnf"),
		"-verify", "-in", "testreq.pem", "-noout"])),
       "Verifying signature on request");
};

my @openssl_args = ("req", "-config", srctop_file("apps", "openssl.cnf"));

run_conversion('req conversions',
	       "testreq.pem");
run_conversion('req conversions -- testreq2',
	       srctop_file("test", "testreq2.pem"));

unlink "testkey.pem", "testreq.pem";

sub run_conversion {
    my $title = shift;
    my $reqfile = shift;

    subtest $title => sub {
	run(app(["openssl", @openssl_args,
		 "-in", $reqfile, "-inform", "p",
		 "-noout", "-text"],
		stderr => "req-check.err", stdout => undef));
	open DATA, "req-check.err";
      SKIP: {
	  plan skip_all => "skipping req conversion test for $reqfile"
	      if grep /Unknown Public Key/, map { s/\R//; } <DATA>;

	  tconversion("req", $reqfile, @openssl_args);
	}
	close DATA;
	unlink "req-check.err";

	done_testing();
    };
}
