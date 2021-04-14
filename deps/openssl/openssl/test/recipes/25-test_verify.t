#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec::Functions qw/canonpath/;
use File::Copy;
use OpenSSL::Test qw/:DEFAULT srctop_file ok_nofips with/;
use OpenSSL::Test::Utils;

setup("test_verify");

sub verify {
    my ($cert, $purpose, $trusted, $untrusted, @opts) = @_;
    my @path = qw(test certs);
    my @args = qw(openssl verify -auth_level 1);
    push(@args, "-purpose", $purpose) if $purpose ne "";
    push(@args, @opts);
    for (@$trusted) { push(@args, "-trusted", srctop_file(@path, "$_.pem")) }
    for (@$untrusted) { push(@args, "-untrusted", srctop_file(@path, "$_.pem")) }
    push(@args, srctop_file(@path, "$cert.pem"));
    run(app([@args]));
}

plan tests => 159;

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

# Critical extensions

ok(verify("ee-cert-noncrit-unknown-ext", "", ["root-cert"], ["ca-cert"]),
   "accept non-critical unknown extension");
ok(!verify("ee-cert-crit-unknown-ext", "", ["root-cert"], ["ca-cert"]),
   "reject critical unknown extension");
ok(verify("ee-cert-ocsp-nocheck", "", ["root-cert"], ["ca-cert"]),
   "accept critical OCSP No Check");

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
ok(!verify("ee-cert", "sslserver", [qw(ca-expired)], [], "-partial_chain"),
   "reject expired trusted partial chain"); # this check is beyond RFC 5280
ok(!verify("ee-cert", "sslserver", [qw(root-expired)], [qw(ca-cert)]),
   "reject expired trusted root"); # this check is beyond RFC 5280
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
ok(verify("ee-cert", "sslserver", [qw(root-cert sca+serverAuth)], [qw(ca-cert)]),
   "accept server trust and purpose");
ok(verify("ee-cert", "sslserver", [qw(root-cert sca+anyEKU)], [qw(ca-cert)]),
   "accept wildcard trust and server purpose");
ok(verify("ee-cert", "sslserver", [qw(root-cert sca-clientAuth)], [qw(ca-cert)]),
   "accept client mistrust and server purpose");
ok(verify("ee-cert", "sslserver", [qw(root-cert cca+serverAuth)], [qw(ca-cert)]),
   "accept server trust and client purpose");
ok(verify("ee-cert", "sslserver", [qw(root-cert cca+anyEKU)], [qw(ca-cert)]),
   "accept wildcard trust and client purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert cca-cert)], [qw(ca-cert)]),
   "fail client purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert ca-anyEKU)], [qw(ca-cert)]),
   "fail wildcard mistrust");
ok(!verify("ee-cert", "sslserver", [qw(root-cert ca-serverAuth)], [qw(ca-cert)]),
   "fail server mistrust");
ok(!verify("ee-cert", "sslserver", [qw(root-cert ca+clientAuth)], [qw(ca-cert)]),
   "fail client trust");
ok(!verify("ee-cert", "sslserver", [qw(root-cert sca+clientAuth)], [qw(ca-cert)]),
   "fail client trust and server purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert cca+clientAuth)], [qw(ca-cert)]),
   "fail client trust and client purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert cca-serverAuth)], [qw(ca-cert)]),
   "fail server mistrust and client purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert cca-clientAuth)], [qw(ca-cert)]),
   "fail client mistrust and client purpose");
ok(!verify("ee-cert", "sslserver", [qw(root-cert sca-serverAuth)], [qw(ca-cert)]),
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
ok(verify("ee-pathlen", "sslserver", [qw(root-cert)], [qw(ca-cert)]),
   "accept non-ca with pathlen:0 by default");
ok(!verify("ee-pathlen", "sslserver", [qw(root-cert)], [qw(ca-cert)], "-x509_strict"),
   "reject non-ca with pathlen:0 with strict flag");

# Proxy certificates
ok(!verify("pc1-cert", "sslclient", [qw(root-cert)], [qw(ee-client ca-cert)]),
   "fail to accept proxy cert without -allow_proxy_certs");
ok(verify("pc1-cert", "sslclient", [qw(root-cert)], [qw(ee-client ca-cert)],
          "-allow_proxy_certs"),
   "accept proxy cert 1");
ok(verify("pc2-cert", "sslclient", [qw(root-cert)], [qw(pc1-cert ee-client ca-cert)],
          "-allow_proxy_certs"),
   "accept proxy cert 2");
ok(!verify("bad-pc3-cert", "sslclient", [qw(root-cert)], [qw(pc1-cert ee-client ca-cert)],
          "-allow_proxy_certs"),
   "fail proxy cert with incorrect subject");
ok(!verify("bad-pc4-cert", "sslclient", [qw(root-cert)], [qw(pc1-cert ee-client ca-cert)],
          "-allow_proxy_certs"),
   "fail proxy cert with incorrect pathlen");
ok(verify("pc5-cert", "sslclient", [qw(root-cert)], [qw(pc1-cert ee-client ca-cert)],
          "-allow_proxy_certs"),
   "accept proxy cert missing proxy policy");
ok(!verify("pc6-cert", "sslclient", [qw(root-cert)], [qw(pc1-cert ee-client ca-cert)],
          "-allow_proxy_certs"),
   "failed proxy cert where last CN was added as a multivalue RDN component");

# Security level tests
ok(verify("ee-cert", "", ["root-cert"], ["ca-cert"], "-auth_level", "2"),
   "accept RSA 2048 chain at auth level 2");
ok(!verify("ee-cert", "", ["root-cert"], ["ca-cert"], "-auth_level", "3"),
   "reject RSA 2048 root at auth level 3");
ok(verify("ee-cert", "", ["root-cert-768"], ["ca-cert-768i"], "-auth_level", "0"),
   "accept RSA 768 root at auth level 0");
ok(!verify("ee-cert", "", ["root-cert-768"], ["ca-cert-768i"]),
   "reject RSA 768 root at auth level 1");
ok(verify("ee-cert-768i", "", ["root-cert"], ["ca-cert-768"], "-auth_level", "0"),
   "accept RSA 768 intermediate at auth level 0");
ok(!verify("ee-cert-768i", "", ["root-cert"], ["ca-cert-768"]),
   "reject RSA 768 intermediate at auth level 1");
ok(verify("ee-cert-768", "", ["root-cert"], ["ca-cert"], "-auth_level", "0"),
   "accept RSA 768 leaf at auth level 0");
ok(!verify("ee-cert-768", "", ["root-cert"], ["ca-cert"]),
   "reject RSA 768 leaf at auth level 1");
#
ok(verify("ee-cert", "", ["root-cert-md5"], ["ca-cert"], "-auth_level", "2"),
   "accept md5 self-signed TA at auth level 2");
ok(verify("ee-cert", "", ["ca-cert-md5-any"], [], "-auth_level", "2"),
   "accept md5 intermediate TA at auth level 2");
ok(verify("ee-cert", "", ["root-cert"], ["ca-cert-md5"], "-auth_level", "0"),
   "accept md5 intermediate at auth level 0");
ok(!verify("ee-cert", "", ["root-cert"], ["ca-cert-md5"]),
   "reject md5 intermediate at auth level 1");
ok(verify("ee-cert-md5", "", ["root-cert"], ["ca-cert"], "-auth_level", "0"),
   "accept md5 leaf at auth level 0");
ok(!verify("ee-cert-md5", "", ["root-cert"], ["ca-cert"]),
   "reject md5 leaf at auth level 1");

# Explicit vs named curve tests
SKIP: {
    skip "EC is not supported by this OpenSSL build", 3
        if disabled("ec");
    ok(!verify("ee-cert-ec-explicit", "", ["root-cert"],
               ["ca-cert-ec-named"]),
        "reject explicit curve leaf with named curve intermediate");
    ok(!verify("ee-cert-ec-named-explicit", "", ["root-cert"],
               ["ca-cert-ec-explicit"]),
        "reject named curve leaf with explicit curve intermediate");
    ok(verify("ee-cert-ec-named-named", "", ["root-cert"],
              ["ca-cert-ec-named"]),
        "accept named curve leaf with named curve intermediate");
}

# Depth tests, note the depth limit bounds the number of CA certificates
# between the trust-anchor and the leaf, so, for example, with a root->ca->leaf
# chain, depth = 1 is sufficient, but depth == 0 is not.
#
ok(verify("ee-cert", "", ["root-cert"], ["ca-cert"], "-verify_depth", "2"),
   "accept chain with verify_depth 2");
ok(verify("ee-cert", "", ["root-cert"], ["ca-cert"], "-verify_depth", "1"),
   "accept chain with verify_depth 1");
ok(!verify("ee-cert", "", ["root-cert"], ["ca-cert"], "-verify_depth", "0"),
   "reject chain with verify_depth 0");
ok(verify("ee-cert", "", ["ca-cert-md5-any"], [], "-verify_depth", "0"),
   "accept md5 intermediate TA with verify_depth 0");

# Name Constraints tests.

ok(verify("alt1-cert", "", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints everything permitted");

ok(verify("alt2-cert", "", ["root-cert"], ["ncca2-cert"], ),
   "Name Constraints nothing excluded");

ok(verify("alt3-cert", "", ["root-cert"], ["ncca1-cert", "ncca3-cert"], ),
   "Name Constraints nested test all permitted");

ok(verify("goodcn1-cert", "", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints CNs permitted");

ok(!verify("badcn1-cert", "", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints CNs not permitted");

ok(!verify("badalt1-cert", "", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints hostname not permitted");

ok(!verify("badalt2-cert", "", ["root-cert"], ["ncca2-cert"], ),
   "Name Constraints hostname excluded");

ok(!verify("badalt3-cert", "", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints email address not permitted");

ok(!verify("badalt4-cert", "", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints subject email address not permitted");

ok(!verify("badalt5-cert", "", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints IP address not permitted");

ok(!verify("badalt6-cert", "", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints CN hostname not permitted");

ok(!verify("badalt7-cert", "", ["root-cert"], ["ncca1-cert"], ),
   "Name Constraints CN BMPSTRING hostname not permitted");

ok(!verify("badalt8-cert", "", ["root-cert"], ["ncca1-cert", "ncca3-cert"], ),
   "Name constraints nested DNS name not permitted 1");

ok(!verify("badalt9-cert", "", ["root-cert"], ["ncca1-cert", "ncca3-cert"], ),
   "Name constraints nested DNS name not permitted 2");

ok(!verify("badalt10-cert", "", ["root-cert"], ["ncca1-cert", "ncca3-cert"], ),
   "Name constraints nested DNS name excluded");

#Check that we get the expected failure return code
with({ exit_checker => sub { return shift == 2; } },
     sub {
         ok(verify("bad-othername-namec", "", ["bad-othername-namec-inter"], [],
                   "-partial_chain", "-attime", "1623060000"),
            "Name constraints bad othername name constraint");
     });

ok(verify("ee-pss-sha1-cert", "", ["root-cert"], ["ca-cert"], "-auth_level", "0"),
    "Accept PSS signature using SHA1 at auth level 0");

ok(verify("ee-pss-sha256-cert", "", ["root-cert"], ["ca-cert"], ),
    "CA with PSS signature using SHA256");

ok(!verify("ee-pss-sha1-cert", "", ["root-cert"], ["ca-cert"], "-auth_level", "1"),
    "Reject PSS signature using SHA1 and auth level 1");

ok(verify("ee-pss-sha256-cert", "", ["root-cert"], ["ca-cert"], "-auth_level", "2"),
    "PSS signature using SHA256 and auth level 2");

ok(verify("ee-pss-cert", "", ["root-cert"], ["ca-pss-cert"], ),
    "CA PSS signature");
ok(!verify("ee-pss-wrong1.5-cert", "", ["root-cert"], ["ca-pss-cert"], ),
    "CA producing regular PKCS#1 v1.5 signature with PSA-PSS key");

ok(!verify("many-names1", "", ["many-constraints"], ["many-constraints"], ),
    "Too many names and constraints to check (1)");
ok(!verify("many-names2", "", ["many-constraints"], ["many-constraints"], ),
    "Too many names and constraints to check (2)");
ok(!verify("many-names3", "", ["many-constraints"], ["many-constraints"], ),
    "Too many names and constraints to check (3)");

ok(verify("some-names1", "", ["many-constraints"], ["many-constraints"], ),
    "Not too many names and constraints to check (1)");
ok(verify("some-names2", "", ["many-constraints"], ["many-constraints"], ),
    "Not too many names and constraints to check (2)");
ok(verify("some-names2", "", ["many-constraints"], ["many-constraints"], ),
    "Not too many names and constraints to check (3)");
ok(verify("root-cert-rsa2", "", ["root-cert-rsa2"], [], "-check_ss_sig"),
    "Public Key Algorithm rsa instead of rsaEncryption");

ok(verify("ee-self-signed", "", ["ee-self-signed"], [], "-attime", "1593565200"),
   "accept trusted self-signed EE cert excluding key usage keyCertSign");
ok(verify("ee-ss-with-keyCertSign", "", ["ee-ss-with-keyCertSign"], []),
   "accept trusted self-signed EE cert with key usage keyCertSign also when strict");

SKIP: {
    skip "Ed25519 is not supported by this OpenSSL build", 6
        if disabled("ec");

    # ED25519 certificate from draft-ietf-curdle-pkix-04
    ok(verify("ee-ed25519", "", ["root-ed25519"], []),
       "accept X25519 EE cert issued by trusted Ed25519 self-signed CA cert");

    ok(!verify("ee-ed25519", "", ["root-ed25519"], [], "-x509_strict"),
       "reject X25519 EE cert in strict mode since AKID is missing");

    ok(!verify("root-ed25519", "", ["ee-ed25519"], []),
       "fail Ed25519 CA and EE certs swapped");

    ok(verify("root-ed25519", "", ["root-ed25519"], []),
       "accept trusted Ed25519 self-signed CA cert");

    ok(!verify("ee-ed25519", "", ["ee-ed25519"], []),
       "fail trusted Ed25519-signed self-issued X25519 cert");

    ok(verify("ee-ed25519", "", ["ee-ed25519"], [], "-partial_chain"),
       "accept last-resort direct leaf match Ed25519-signed self-issued cert");

}

SKIP: {
    skip "SM2 is not supported by this OpenSSL build", 2 if disabled("sm2");

   ok_nofips(verify("sm2", "", ["sm2-ca-cert"], [], "-vfyopt", "distid:1234567812345678"),
       "SM2 ID test");
   ok_nofips(verify("sm2", "", ["sm2-ca-cert"], [], "-vfyopt", "hexdistid:31323334353637383132333435363738"),
       "SM2 hex ID test");
}

# Mixed content tests
my $cert_file = srctop_file('test', 'certs', 'root-cert.pem');
my $rsa_file = srctop_file('test', 'certs', 'key-pass-12345.pem');

SKIP: {
    my $certplusrsa_file = 'certplusrsa.pem';
    my $certplusrsa;

    skip "Couldn't create certplusrsa.pem", 1
        unless ( open $certplusrsa, '>', $certplusrsa_file
                 and copy($cert_file, $certplusrsa)
                 and copy($rsa_file, $certplusrsa)
                 and close $certplusrsa );

    ok(run(app([ qw(openssl verify -trusted), $certplusrsa_file, $cert_file ])),
       'Mixed cert + key file test');
}

SKIP: {
    my $rsapluscert_file = 'rsapluscert.pem';
    my $rsapluscert;

    skip "Couldn't create rsapluscert.pem", 1
        unless ( open $rsapluscert, '>', $rsapluscert_file
                 and copy($rsa_file, $rsapluscert)
                 and copy($cert_file, $rsapluscert)
                 and close $rsapluscert );

    ok(run(app([ qw(openssl verify -trusted), $rsapluscert_file, $cert_file ])),
       'Mixed key + cert file test');
}
