#! /usr/bin/env perl
# Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw/:DEFAULT with bldtop_file srctop_file cmdstr/;
use OpenSSL::Test::Utils;

setup("test_verify_store");

plan tests => 10;

my $dummycnf = srctop_file("apps", "openssl.cnf");
my $cakey = srctop_file("test", "certs", "ca-key.pem");
my $ukey = srctop_file("test", "certs", "ee-key.pem");

my $cnf = srctop_file("test", "ca-and-certs.cnf");
my $CAkey = "keyCA.ss";
my $CAcert="certCA.ss";
my $CAserial="certCA.srl";
my $CAreq="reqCA.ss";
my $CAreq2="req2CA.ss"; # temp
my $Ukey="keyU.ss";
my $Ureq="reqU.ss";
my $Ucert="certU.ss";

SKIP: {
    req( 'make cert request',
         qw(-new -section userreq),
         -config       => $cnf,
         -out          => $CAreq,
         -key          => $cakey,
         -keyout       => $CAkey );

    skip 'failure', 8 unless
        x509( 'convert request into self-signed cert',
              qw(-req -CAcreateserial -days 30),
              qw(-extensions v3_ca),
              -in       => $CAreq,
              -out      => $CAcert,
              -signkey  => $CAkey,
              -extfile  => $cnf );

    skip 'failure', 7 unless
        x509( 'convert cert into a cert request',
              qw(-x509toreq),
              -in       => $CAcert,
              -out      => $CAreq2,
              -signkey  => $CAkey );

    skip 'failure', 6 unless
        req( 'verify request 1',
             qw(-verify -noout -section userreq),
             -config    => $dummycnf,
             -in        => $CAreq );

    skip 'failure', 5 unless
        req( 'verify request 2',
             qw(-verify -noout -section userreq),
             -config    => $dummycnf,
             -in        => $CAreq2 );

    skip 'failure', 4 unless
        verify( 'verify signature',
                -CAstore => $CAcert,
                $CAcert );

    skip 'failure', 3 unless
        req( 'make a user cert request',
             qw(-new -section userreq),
             -config  => $cnf,
             -out     => $Ureq,
             -key     => $ukey,
             -keyout  => $Ukey );

    skip 'failure', 2 unless
        x509( 'sign user cert request',
              qw(-req -CAcreateserial -days 30 -extensions v3_ee),
              -in     => $Ureq,
              -out    => $Ucert,
              -CA     => $CAcert,
              -CAkey  => $CAkey,
              -CAserial => $CAserial,
              -extfile => $cnf )
        && verify( undef,
                   -CAstore => $CAcert,
                   $Ucert );

    skip 'failure', 0 unless
        x509( 'Certificate details',
              qw(-subject -issuer -startdate -enddate -noout),
              -in     => $Ucert );
}

sub verify {
    my $title = shift;

    ok(run(app([qw(openssl verify), @_])), $title);
}

sub req {
    my $title = shift;

    ok(run(app([qw(openssl req), @_])), $title);
}

sub x509 {
    my $title = shift;

    ok(run(app([qw(openssl x509), @_])), $title);
}
