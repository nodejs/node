#! /usr/bin/env perl
# Copyright 2020-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test qw(:DEFAULT data_file srctop_file);
use OpenSSL::Test::Utils;

#Tests for the dhparam CLI application

setup("test_dhparam");

plan skip_all => "DH is not supported in this build"
    if disabled("dh");
plan tests => 21;

my $fipsconf = srctop_file("test", "fips-and-base.cnf");

sub checkdhparams {
    my $file = shift; #Filename containing params
    my $type = shift; #PKCS3 or X9.42?
    my $gen = shift; #2, 5 or something else (0 is "something else")?
    my $format = shift; #DER or PEM?
    my $bits = shift; #Number of bits in p
    my $pemtype;
    my $readtype;
    my $readbits = 0;
    my $genline;

    if (-T $file) {
        #Text file. Check it looks like PEM
        open(PEMFILE, '<', $file) or die $!;
        if (my $firstline = <PEMFILE>) {
            $firstline =~ s/\R$//;
            if ($firstline eq "-----BEGIN DH PARAMETERS-----") {
                $pemtype = "PKCS3";
            } elsif ($firstline eq "-----BEGIN X9.42 DH PARAMETERS-----") {
                $pemtype = "X9.42";
            } else {
                $pemtype = "";
            }
        } else {
            $pemtype = "";
        }
        close(PEMFILE);
        ok(($format eq "PEM") && defined $pemtype, "Checking format is PEM");
    } else {
        ok($format eq "DER", "Checking format is DER");
        #No PEM type in this case, so we just set the pemtype to the expected
        #type so that we never fail that part of the test
        $pemtype = $type;
    }
    my @textdata = run(app(['openssl', 'dhparam', '-in', $file, '-noout',
                            '-text', '-inform', $format]), capture => 1);
    chomp(@textdata);
    #Trim trailing whitespace
    @textdata = grep { s/\s*$//g } @textdata;
    if (grep { $_ =~ 'Q:' } @textdata) {
        $readtype = "X9.42";
    } else {
        $readtype = "PKCS3";
    }
    ok(($type eq $pemtype) && ($type eq $readtype),
       "Checking parameter type is ".$type." ($pemtype, $readtype)");

    if (defined $textdata[0] && $textdata[0] =~ /DH Parameters: \((\d+) bit\)/) {
        $readbits = $1;
    }
    ok($bits == $readbits, "Checking number of bits is $bits");
    if ($gen == 2 || $gen == 5) {
        #For generators 2 and 5 the value appears on the same line
        $genline = "G:    $gen (0x$gen)";
    } else {
        #For any other generator the value appears on the following line
        $genline = "G:";
    }

    ok((grep { (index($_, $genline) + length ($genline)) == length ($_)} @textdata),
       "Checking generator is correct");
}

#Test some "known good" parameter files to check that we can read them
subtest "Read: 1024 bit PKCS3 params, generator 2, PEM file" => sub {
    plan tests => 4;
    checkdhparams(data_file("pkcs3-2-1024.pem"), "PKCS3", 2, "PEM", 1024);
};
subtest "Read: 1024 bit PKCS3 params, generator 5, PEM file" => sub {
    plan tests => 4;
    checkdhparams(data_file("pkcs3-5-1024.pem"), "PKCS3", 5, "PEM", 1024);
};
subtest "Read: 2048 bit PKCS3 params, generator 2, PEM file" => sub {
    plan tests => 4;
    checkdhparams(data_file("pkcs3-2-2048.pem"), "PKCS3", 2, "PEM", 2048);
};
subtest "Read: 1024 bit X9.42 params, PEM file" => sub {
    plan tests => 4;
    checkdhparams(data_file("x942-0-1024.pem"), "X9.42", 0, "PEM", 1024);
};
subtest "Read: 1024 bit PKCS3 params, generator 2, DER file" => sub {
    plan tests => 4;
    checkdhparams(data_file("pkcs3-2-1024.der"), "PKCS3", 2, "DER", 1024);
};
subtest "Read: 1024 bit PKCS3 params, generator 5, DER file" => sub {
    plan tests => 4;
    checkdhparams(data_file("pkcs3-5-1024.der"), "PKCS3", 5, "DER", 1024);
};
subtest "Read: 2048 bit PKCS3 params, generator 2, DER file" => sub {
    plan tests => 4;
    checkdhparams(data_file("pkcs3-2-2048.der"), "PKCS3", 2, "DER", 2048);
};
subtest "Read: 1024 bit X9.42 params, DER file" => sub {
    checkdhparams(data_file("x942-0-1024.der"), "X9.42", 0, "DER", 1024);
};

#Test that generating parameters of different types creates what we expect. We
#use 512 for the size for speed reasons. Don't use this in real applications!
subtest "Generate: 512 bit PKCS3 params, generator 2, PEM file" => sub {
    plan tests => 5;
    ok(run(app([ 'openssl', 'dhparam', '-out', 'gen-pkcs3-2-512.pem',
                 '512' ])));
    checkdhparams("gen-pkcs3-2-512.pem", "PKCS3", 2, "PEM", 512);
};
subtest "Generate: 512 bit PKCS3 params, explicit generator 2, PEM file" => sub {
    plan tests => 5;
    ok(run(app([ 'openssl', 'dhparam', '-out', 'gen-pkcs3-exp2-512.pem', '-2',
                 '512' ])));
    checkdhparams("gen-pkcs3-exp2-512.pem", "PKCS3", 2, "PEM", 512);
};
subtest "Generate: 512 bit PKCS3 params, generator 5, PEM file" => sub {
    plan tests => 5;
    ok(run(app([ 'openssl', 'dhparam', '-out', 'gen-pkcs3-5-512.pem', '-5',
                 '512' ])));
    checkdhparams("gen-pkcs3-5-512.pem", "PKCS3", 5, "PEM", 512);
};
subtest "Generate: 512 bit PKCS3 params, generator 2, explicit PEM file" => sub {
    plan tests => 5;
    ok(run(app([ 'openssl', 'dhparam', '-out', 'gen-pkcs3-2-512.exp.pem',
                 '-outform', 'PEM', '512' ])));
    checkdhparams("gen-pkcs3-2-512.exp.pem", "PKCS3", 2, "PEM", 512);
};
SKIP: {
    skip "Skipping tests that require DSA", 4 if disabled("dsa");

    subtest "Generate: 512 bit X9.42 params, generator 0, PEM file" => sub {
        plan tests => 5;
        ok(run(app([ 'openssl', 'dhparam', '-out', 'gen-x942-0-512.pem',
                    '-dsaparam', '512' ])));
        checkdhparams("gen-x942-0-512.pem", "X9.42", 0, "PEM", 512);
    };
    subtest "Generate: 512 bit X9.42 params, explicit generator 2, PEM file" => sub {
        plan tests => 1;
        #Expected to fail - you cannot select a generator with '-dsaparam'
        ok(!run(app([ 'openssl', 'dhparam', '-out', 'gen-x942-exp2-512.pem', '-2',
                    '-dsaparam', '512' ])));
    };
    subtest "Generate: 512 bit X9.42 params, generator 5, PEM file" => sub {
        plan tests => 1;
        #Expected to fail - you cannot select a generator with '-dsaparam'
        ok(!run(app([ 'openssl', 'dhparam', '-out', 'gen-x942-5-512.pem',
                    '-5', '-dsaparam', '512' ])));
    };
    subtest "Generate: 512 bit X9.42 params, generator 0, DER file" => sub {
        plan tests => 5;
        ok(run(app([ 'openssl', 'dhparam', '-out', 'gen-x942-0-512.der',
                    '-dsaparam', '-outform', 'DER', '512' ])));
        checkdhparams("gen-x942-0-512.der", "X9.42", 0, "DER", 512);
    };
}
SKIP: {
    skip "Skipping tests that are only supported in a fips build with security ".
        "checks", 4 if (disabled("fips") || disabled("fips-securitychecks"));

    $ENV{OPENSSL_CONF} = $fipsconf;

    ok(!run(app(['openssl', 'dhparam', '-check', '512'])),
        "Generating 512 bit DH params should fail in FIPS mode");

    ok(run(app(['openssl', 'dhparam', '-provider', 'default', '-propquery',
            '?fips!=yes', '-check', '512'])),
        "Generating 512 bit DH params should succeed in FIPS mode using".
        " non-FIPS property query");

    SKIP: {
        skip "Skipping tests that require DSA", 2 if disabled("dsa");

        ok(!run(app(['openssl', 'dhparam', '-dsaparam', '-check', '512'])),
            "Generating 512 bit DSA-style DH params should fail in FIPS mode");

        ok(run(app(['openssl', 'dhparam', '-provider', 'default', '-propquery',
                '?fips!=yes', '-dsaparam', '-check', '512'])),
            "Generating 512 bit DSA-style DH params should succeed in FIPS".
            " mode using non-FIPS property query");
    }

    delete $ENV{OPENSSL_CONF};
}

ok(run(app(["openssl", "dhparam", "-noout", "-text"],
           stdin => data_file("pkcs3-2-1024.pem"))),
   "stdinbuffer input test that uses BIO_gets");
