#! /usr/bin/env perl

# Copyright 2018-2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;

use OpenSSL::Test qw(:DEFAULT srctop_file);
use OpenSSL::Test::Utils;

setup("test_algorithmid");

# eecert => cacert
my %certs_info =
    (
     'ee-cert' => 'ca-cert',
     'ee-cert2' => 'ca-cert2',

     # 'ee-pss-sha1-cert' => 'ca-cert',
     # 'ee-pss-sha256-cert' => 'ca-cert',
     # 'ee-pss-cert' => 'ca-pss-cert',
     # 'server-pss-restrict-cert' => 'rootcert',

     (
      disabled('ec')
      ? ()
      : (
         'ee-cert-ec-explicit' => 'ca-cert-ec-named',
         'ee-cert-ec-named-explicit' => 'ca-cert-ec-explicit',
         'ee-cert-ec-named-named' => 'ca-cert-ec-named',
         # 'server-ed448-cert' => 'root-ed448-cert'
         'server-ecdsa-brainpoolP256r1-cert' => 'rootcert',
        )
     )
    );
my @pubkeys =
    (
     'testrsapub',
     disabled('dsa') ? () : 'testdsapub',
     disabled('ec') ? () : qw(testecpub-p256),
     disabled('ecx') ? () : qw(tested25519pub tested448pub)
    );
my @certs = sort keys %certs_info;

plan tests =>
    scalar @certs
    + scalar @pubkeys;

foreach (@certs) {
    ok(run(test(['algorithmid_test', '-x509',
                 srctop_file('test', 'certs', "$_.pem"),
                 srctop_file('test', 'certs', "$certs_info{$_}.pem")])));
}

foreach (sort @pubkeys) {
    ok(run(test(['algorithmid_test', '-spki', srctop_file('test', "$_.pem")])));
}
