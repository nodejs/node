#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec::Functions qw/canonpath/;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_verify");

sub verify {
    my ($cert, $purpose, $trusted, $untrusted, @opts) = @_;
    my @args = qw(openssl verify -auth_level 1 -purpose);
    my @path = qw(test certs);
    push(@args, "$purpose", @opts);
    for (@$trusted) {
        push(@args, "-trusted", srctop_file(@path, "$_.pem"))
    }
    for (@$untrusted) {
        push(@args, "-untrusted", srctop_file(@path, "$_.pem"))
    }
    push(@args, srctop_file(@path, "$cert.pem"));
    run(app([@args]));
}

plan tests => 127;

# Canonical success
ok(verify("ee-cert", "sslserver", ["root-cert"], ["ca-cert"]),
   "accept compat trust");

# Root CA variants
ok(!verify("ee-cert", "sslserver", [qw(root-nonca)], [qw(ca-cert)]),
   "fail trusted non-ca root");
ok(!verify("ee-cert", "sslserver", [qw(nroot+serverAuth)], [qw(ca-cert)]),
   "fail server trust non-ca root");
ok(!verify("ee-cert", "sslserver", [qw(nroot+anyEKU)], [qw(ca-cert)]),
   "fail wildcard trust non-ca root");
ok(!verify("ee-cert", "sslserver", [qw(root-cert2)], [qw(ca-cert)]),
   "fail wrong root key");
ok(!verify("ee-cert", "sslserver", [qw(root-name2)], [qw(ca-cert)]),
   "fail wrong root DN");

# Explicit trust/purpose combinations
#
ok(verify("ee-cert", "sslserver", [qw(sroot-cert)], [qw(ca-cert)]),
   "accept server purpose");
ok(!verify("ee-cert", "sslserver", [qw(croot-cert)], [qw(ca-cert)]),
   "fail client purpose");
ok(verify("ee-cert", "sslserver", [qw(root+serverAuth)], [qw(ca-cert)]),
   "accept server trust");
ok(verify("ee-cert", "sslserver", [qw(sroot+serverAuth)], [qw(ca-cert)]),
   "accept server trust with server purpose");
ok(verify("ee-cert", "sslserver", [qw(croot+serverAuth)], [qw(ca-cert)]),
   "accept server trust with client purpose");
# Wildcard trust
ok(verify("ee-cert", "sslserver", [qw(root+anyEKU)], [qw(ca-cert)]),
   "accept wildcard trust");
ok(verify("ee-cert", "sslserver", [qw(sroot+anyEKU)], [qw(ca-cert)]),
   "accept wildcard trust with server purpose");
ok(verify("ee-cert", "sslserver", [qw(croot+anyEKU)], [qw(ca-cert)]),
   "accept wildcard trust with client purpose");
# Inapplicable mistrust
ok(verify("ee-cert", "sslserver", [qw(root-clientAuth)], [qw(ca-cert)]),
   "accept client mistrust");
ok(verify("ee-cert", "sslserver", [qw(sroot-clientAuth)], [qw(ca-cert)]),
   "accept client mistrust with server purpose");
ok(!verify("ee-cert", "sslserver", [qw(croot-clientAuth)], [qw(ca-cert)]),
   "fail client mistrust with client purpose");
# Inapplicable trust
ok(!verify("ee-cert", "sslserver", [qw(root+clientAuth)], [qw(ca-cert)]),
   "fail client trust");
ok(!verify("ee-cert", "sslserver", [qw(sroot+clientAuth)], [qw(ca-cert)]),
   "fail client trust with server purpose");
ok(!verify("ee-cert", "sslserver", [qw(croot+clientAuth)], [qw(ca-cert)]),
   "fail client trust with client purpose");
# Server mistrust
ok(!verify("ee-cert", "sslserver", [qw(root-serverAuth)], [qw(ca-cert)]),
   "fail rejected EKU");
ok(!verify("ee-cert", "sslserver", [qw(sroot-serverAuth)], [qw(ca-cert)]),
   "fail server mistrust with server purpose");
ok(!verify("ee-cert", "sslserver", [qw(croot-serverAuth)], [qw(ca-cert)]),
   "fail server mistrust with client purpose");
# Wildcard mistrust
ok(!verify("ee-cert", "sslserver", [qw(root-anyEKU)], [qw(ca-cert)]),
   "fail wildcard mistrust");
ok(!verify("ee-cert", "sslserver", [qw(sroot-anyEKU)], [qw(ca-cert)]),
   "fail wildcard mistrust with server purpose");
ok(!verify("ee-cert", "sslserver", [qw(croot-anyEKU)], [qw(ca-cert)]),
   "fail wildcard mistrust with client purpose");

# Check that trusted-first is on by setting up paths to different roots
# depending on whether the intermediate is the trusted or untrusted one.
#
ok(verify("ee-cert", "sslserver", [qw(root-serverAuth root-cert2 ca-root2)],
          [qw(ca-cert)]),
   "accept trusted-first path");
ok(verify("ee-cert", "sslserver", [qw(root-cert root2+serverAuth ca-root2)],
          [qw(ca-cert)]),
   "accept trusted-first path with server trust");
ok(!verify("ee-cert", "sslserver", [qw(root-cert root2-serverAuth ca-root2)],
           [qw(ca-cert)]),
   "fail trusted-first path with server mistrust");
ok(!verify("ee-cert", "sslserver", [qw(root-cert root2+clientAuth ca-root2)],
           [qw(ca-cert)]),
   "fail trusted-first path with client trust");

# CA variants
ok(!verify("ee-cert", "sslserver", [qw(root-cert)], [qw(ca-nonca)]),
   "fail non-CA untrusted intermediate");
ok(!verify("ee-cert", "sslserver", [qw(root-cert)], [qw(ca-nonbc)]),
   "fail non-CA untrusted intermediate");
ok(!verify("ee-cert", "sslserver", [qw(root-cert ca-nonca)], []),
   "fail non-CA trust-store intermediate");
ok(!verify("ee-cert", "sslserver", [qw(root-cert ca-nonbc)], []),
   "fail non-CA trust-store intermediate");
ok(!verify("ee-cert", "sslserver", [qw(root-cert nca+serverAuth)], []),
   "fail non-CA server trust intermediate");
ok(!verify("ee-cert", "sslserver", [qw(root-cert nca+anyEKU)], []),
   "fail non-CA wildcard trust intermediate");
ok(!verify("ee-cert", "sslserver", [qw(root-cert)], [qw(ca-cert2)]),
   "fail wrong intermediate CA key");
ok(!verify("ee-cert", "sslserver", [qw(root-cert)], [qw(ca-name2)]),
   "fail wrong intermediate CA DN");
ok(!verify("ee-cert", "sslserver", [qw(root-cert)], [qw(ca-root2)]),
   "fail wrong intermediate CA issuer");
ok(!verify("ee-cert", "sslserver", [], [qw(ca-cert)], "-partial_chain"),
   "fail untrusted partial chain");
ok(verify("ee-cert", "sslserver", [qw(ca-cert)], [], "-partial_chain"),
   "accept trusted partial chain");
ok(verify("ee-cert", "sslserver", [qw(sca-cert)], [], "-partial_chain"),
   "accept partial chain with server purpose");
ok(!verify("ee-cert", "sslserver", [qw(cca-cert)], [], "-partial_chain"),
   "fail partial chain with client purpose");
ok(verify("ee-cert", "sslserver", [qw(ca+serverAuth)], [], "-partial_chain"),
   "accept server trust partial chain");
ok(verify("ee-cert", "sslserver", [qw(cca+serverAuth)], [], "-partial_chain"),
   "accept server trust client purpose partial chain");
ok(verify("ee-cert", "sslserver", [qw(ca-clientAuth)], [], "-partial_chain"),
   "accept client mistrust partial chain");
ok(verify("ee-cert", "sslserver", [qw(ca+anyEKU)], [], "-partial_chain"),
   "accept wildcard trust partial chain");
ok(!verify("ee-cert", "sslserver", [], [qw(ca+serverAuth)], "-partial_chain"),
   "fail untrusted partial issuer with ignored server trust");
ok(!verify("ee-cert", "sslserver", [qw(ca-serverAuth)], [], "-partial_chain"),
   "fail server mistrust partial chain");
ok(!verify("ee-cert", "sslserver", [qw(ca+clientAuth)], [], "-partial_chain"),
   "fail client trust partial chain");
ok(!verify("ee-cert", "sslserver", [qw(ca-anyEKU)], [], "-partial_chain"),
   "fail wildcard mistrust partial chain");

# We now test auxiliary trust even for intermediate trusted certs without
# -partial_chain.  Note that "-trusted_first" is now always on and cannot
# be disabled.
ok(verify("ee-cert", "sslserver", [qw(root-cert ca+serverAuth)], [qw(ca-cert)]),
   "accept server trust");
ok(verify("ee-cert", "sslserver", [qw(root-cert ca+anyEKU)], [qw(ca-cert)]),
   "accept wildcard trust");
ok(verify("ee-cert", "sslserver", [qw(root-cert sca-cert)], [qw(ca-cert)]),
   "accept server purpose");
ok(verify("ee-cert", "sslserver", [qw(root-cert sca+serverAuth)],
          [qw(ca-cert)]),
   "accept server trust and purpose");
ok(verify("ee-cert", "sslserver", [qw(root-cert sca+anyEKU)], [qw(ca-cert)]),
   "accept wildcard trust and server purpose");
ok(verify("ee-cert", "sslserver", [qw(root-cert sca-clientAuth)],
          [qw(ca-cert)]),
   "accept client mistrust and server purpose");
ok(verify("ee-cert", "sslserver", [qw(root-cert cca+serverAuth)],
          [qw(ca-cert)]),
   "accept server trust and client purpose");
ok(verify("ee-cert", "sslserver", [qw(root-cert cca+anyEKU)], [qw(ca-cert)]),
   "accept wildcard trust and client purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert cca-cert)], [qw(ca-cert)]),
   "fail client purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert ca-anyEKU)], [qw(ca-cert)]),
   "fail wildcard mistrust");
ok(!verify("ee-cert", "sslserver", [qw(root-cert ca-serverAuth)],
           [qw(ca-cert)]),
   "fail server mistrust");
ok(!verify("ee-cert", "sslserver", [qw(root-cert ca+clientAuth)],
           [qw(ca-cert)]),
   "fail client trust");
ok(!verify("ee-cert", "sslserver", [qw(root-cert sca+clientAuth)],
           [qw(ca-cert)]),
   "fail client trust and server purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert cca+clientAuth)],
           [qw(ca-cert)]),
   "fail client trust and client purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert cca-serverAuth)],
           [qw(ca-cert)]),
   "fail server mistrust and client purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert cca-clientAuth)],
           [qw(ca-cert)]),
   "fail client mistrust and client purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert sca-serverAuth)],
           [qw(ca-cert)]),
   "fail server mistrust and server purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert sca-anyEKU)], [qw(ca-cert)]),
   "fail wildcard mistrust and server purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert cca-anyEKU)], [qw(ca-cert)]),
   "fail wildcard mistrust and client purpose");

# EE variants
ok(verify("ee-client", "sslclient", [qw(root-cert)], [qw(ca-cert)]),
   "accept client chain");
ok(!verify("ee-client", "sslserver", [qw(root-cert)], [qw(ca-cert)]),
   "fail server leaf purpose");
ok(!verify("ee-cert", "sslclient", [qw(root-cert)], [qw(ca-cert)]),
   "fail client leaf purpose");
ok(!verify("ee-cert2", "sslserver", [qw(root-cert)], [qw(ca-cert)]),
   "fail wrong intermediate CA key");
ok(!verify("ee-name2", "sslserver", [qw(root-cert)], [qw(ca-cert)]),
   "fail wrong intermediate CA DN");
ok(!verify("ee-expired", "sslserver", [qw(root-cert)], [qw(ca-cert)]),
   "fail expired leaf");
ok(verify("ee-cert", "sslserver", [qw(ee-cert)], [], "-partial_chain"),
   "accept last-resort direct leaf match");
ok(verify("ee-client", "sslclient", [qw(ee-client)], [], "-partial_chain"),
   "accept last-resort direct leaf match");
ok(!verify("ee-cert", "sslserver", [qw(ee-client)], [], "-partial_chain"),
   "fail last-resort direct leaf non-match");
ok(verify("ee-cert", "sslserver", [qw(ee+serverAuth)], [], "-partial_chain"),
   "accept direct match with server trust");
ok(!verify("ee-cert", "sslserver", [qw(ee-serverAuth)], [], "-partial_chain"),
   "fail direct match with server mistrust");
ok(verify("ee-client", "sslclient", [qw(ee+clientAuth)], [], "-partial_chain"),
   "accept direct match with client trust");
ok(!verify("ee-client", "sslclient", [qw(ee-clientAuth)], [], "-partial_chain"),
   "reject direct match with client mistrust");

# Proxy certificates
ok(!verify("pc1-cert", "sslclient", [qw(root-cert)], [qw(ee-client ca-cert)]),
   "fail to accept proxy cert without -allow_proxy_certs");
ok(verify("pc1-cert", "sslclient", [qw(root-cert)], [qw(ee-client ca-cert)],
          "-allow_proxy_certs"),
   "accept proxy cert 1");
ok(verify("pc2-cert", "sslclient", [qw(root-cert)],
          [qw(pc1-cert ee-client ca-cert)], "-allow_proxy_certs"),
   "accept proxy cert 2");
ok(!verify("bad-pc3-cert", "sslclient", [qw(root-cert)],
           [qw(pc1-cert ee-client ca-cert)], "-allow_proxy_certs"),
   "fail proxy cert with incorrect subject");
ok(!verify("bad-pc4-cert", "sslclient", [qw(root-cert)],
           [qw(pc1-cert ee-client ca-cert)], "-allow_proxy_certs"),
   "fail proxy cert with incorrect pathlen");
ok(verify("pc5-cert", "sslclient", [qw(root-cert)],
          [qw(pc1-cert ee-client ca-cert)], "-allow_proxy_certs"),
   "accept proxy cert missing proxy policy");
ok(!verify("pc6-cert", "sslclient", [qw(root-cert)],
           [qw(pc1-cert ee-client ca-cert)], "-allow_proxy_certs"),
   "failed proxy cert where last CN was added as a multivalue RDN component");

# Security level tests
ok(verify("ee-cert", "sslserver", ["root-cert"], ["ca-cert"],
          "-auth_level", "2"),
   "accept RSA 2048 chain at auth level 2");
ok(!verify("ee-cert", "sslserver", ["root-cert"], ["ca-cert"],
           "-auth_level", "3"),
   "reject RSA 2048 root at auth level 3");
ok(verify("ee-cert", "sslserver", ["root-cert-768"], ["ca-cert-768i"],
          "-auth_level", "0"),
   "accept RSA 768 root at auth level 0");
ok(!verify("ee-cert", "sslserver", ["root-cert-768"], ["ca-cert-768i"]),
   "reject RSA 768 root at auth level 1");
ok(verify("ee-cert-768i", "sslserver", ["root-cert"], ["ca-cert-768"],
          "-auth_level", "0"),
   "accept RSA 768 intermediate at auth level 0");
ok(!verify("ee-cert-768i", "sslserver", ["root-cert"], ["ca-cert-768"]),
   "reject RSA 768 intermediate at auth level 1");
ok(verify("ee-cert-768", "sslserver", ["root-cert"], ["ca-cert"],
          "-auth_level", "0"),
   "accept RSA 768 leaf at auth level 0");
ok(!verify("ee-cert-768", "sslserver", ["root-cert"], ["ca-cert"]),
   "reject RSA 768 leaf at auth level 1");
#
ok(verify("ee-cert", "sslserver", ["root-cert-md5"], ["ca-cert"],
          "-auth_level", "2"),
   "accept md5 self-signed TA at auth level 2");
ok(verify("ee-cert", "sslserver", ["ca-cert-md5-any"], [],
          "-auth_level", "2"),
   "accept md5 intermediate TA at auth level 2");
ok(verify("ee-cert", "sslserver", ["root-cert"], ["ca-cert-md5"],
          "-auth_level", "0"),
   "accept md5 intermediate at auth level 0");
ok(!verify("ee-cert", "sslserver", ["root-cert"], ["ca-cert-md5"]),
   "reject md5 intermediate at auth level 1");
ok(verify("ee-cert-md5", "sslserver", ["root-cert"], ["ca-cert"],
          "-auth_level", "0"),
   "accept md5 leaf at auth level 0");
ok(!verify("ee-cert-md5", "sslserver", ["root-cert"], ["ca-cert"]),
   "reject md5 leaf at auth level 1");

# Depth tests, note the depth limit bounds the number of CA certificates
# between the trust-anchor and the leaf, so, for example, with a root->ca->leaf
# chain, depth = 1 is sufficient, but depth == 0 is not.
#
ok(verify("ee-cert", "sslserver", ["root-cert"], ["ca-cert"],
          "-verify_depth", "2"),
   "accept chain with verify_depth 2");
ok(verify("ee-cert", "sslserver", ["root-cert"], ["ca-cert"],
          "-verify_depth", "1"),
   "accept chain with verify_depth 1");
ok(!verify("ee-cert", "sslserver", ["root-cert"], ["ca-cert"],
           "-verify_depth", "0"),
   "accept chain with verify_depth 0");
ok(verify("ee-cert", "sslserver", ["ca-cert-md5-any"], [],
          "-verify_depth", "0"),
   "accept md5 intermediate TA with verify_depth 0");

# Name Constraints tests.

ok(verify("alt1-cert", "sslserver", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints everything permitted");

ok(verify("alt2-cert", "sslserver", ["root-cert"], ["ncca2-cert"], ),
   "Name Constraints nothing excluded");

ok(verify("alt3-cert", "sslserver", ["root-cert"], ["ncca1-cert", "ncca3-cert"], ),
   "Name Constraints nested test all permitted");

ok(!verify("badalt1-cert", "sslserver", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints hostname not permitted");

ok(!verify("badalt2-cert", "sslserver", ["root-cert"], ["ncca2-cert"], ),
   "Name Constraints hostname excluded");

ok(!verify("badalt3-cert", "sslserver", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints email address not permitted");

ok(!verify("badalt4-cert", "sslserver", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints subject email address not permitted");

ok(!verify("badalt5-cert", "sslserver", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints IP address not permitted");

ok(!verify("badalt6-cert", "sslserver", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints CN hostname not permitted");

ok(!verify("badalt7-cert", "sslserver", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints CN BMPSTRING hostname not permitted");

ok(!verify("badalt8-cert", "sslserver", ["root-cert"],
           ["ncca1-cert", "ncca3-cert"], ),
   "Name constaints nested DNS name not permitted 1");

ok(!verify("badalt9-cert", "sslserver", ["root-cert"],
           ["ncca1-cert", "ncca3-cert"], ),
   "Name constaints nested DNS name not permitted 2");

ok(!verify("badalt10-cert", "sslserver", ["root-cert"],
           ["ncca1-cert", "ncca3-cert"], ),
   "Name constaints nested DNS name excluded");

ok(!verify("many-names1", "sslserver", ["many-constraints"],
           ["many-constraints"], ),
   "Too many names and constraints to check (1)");
ok(!verify("many-names2", "sslserver", ["many-constraints"],
           ["many-constraints"], ),
   "Too many names and constraints to check (2)");
ok(!verify("many-names3", "sslserver", ["many-constraints"],
           ["many-constraints"], ),
   "Too many names and constraints to check (3)");

ok(verify("some-names1", "sslserver", ["many-constraints"],
          ["many-constraints"], ),
   "Not too many names and constraints to check (1)");
ok(verify("some-names2", "sslserver", ["many-constraints"],
          ["many-constraints"], ),
   "Not too many names and constraints to check (2)");
ok(verify("some-names2", "sslserver", ["many-constraints"],
          ["many-constraints"], ),
   "Not too many names and constraints to check (3)");
