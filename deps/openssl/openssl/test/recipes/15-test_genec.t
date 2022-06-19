#! /usr/bin/env perl
# Copyright 2017-2020 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

# 'supported' and 'unsupported' reflect the current state of things.  In
# Test::More terms, 'supported' works exactly like ok(run(whatever)), while
# 'unsupported' wraps that in a TODO: { } block.
#
# The first argument is the test name (this becomes the last argument to
# 'ok')
# The remaining argument are passed unchecked to 'run'.

# 1:    the result of app() or similar, i.e. something you can pass to
sub supported_pass {
    my $str = shift;

    ok(run(@_), $str);
}

sub supported_fail {
    my $str = shift;

    ok(!run(@_), $str);
}

setup("test_genec");

plan skip_all => "This test is unsupported in a no-ec build"
    if disabled("ec");

my @prime_curves = qw(
    secp112r1
    secp112r2
    secp128r1
    secp128r2
    secp160k1
    secp160r1
    secp160r2
    secp192k1
    secp224k1
    secp224r1
    secp256k1
    secp384r1
    secp521r1
    prime192v1
    prime192v2
    prime192v3
    prime239v1
    prime239v2
    prime239v3
    prime256v1
    wap-wsg-idm-ecid-wtls6
    wap-wsg-idm-ecid-wtls7
    wap-wsg-idm-ecid-wtls8
    wap-wsg-idm-ecid-wtls9
    wap-wsg-idm-ecid-wtls12
    brainpoolP160r1
    brainpoolP160t1
    brainpoolP192r1
    brainpoolP192t1
    brainpoolP224r1
    brainpoolP224t1
    brainpoolP256r1
    brainpoolP256t1
    brainpoolP320r1
    brainpoolP320t1
    brainpoolP384r1
    brainpoolP384t1
    brainpoolP512r1
    brainpoolP512t1
);

my @binary_curves = qw(
    sect113r1
    sect113r2
    sect131r1
    sect131r2
    sect163k1
    sect163r1
    sect163r2
    sect193r1
    sect193r2
    sect233k1
    sect233r1
    sect239k1
    sect283k1
    sect283r1
    sect409k1
    sect409r1
    sect571k1
    sect571r1
    c2pnb163v1
    c2pnb163v2
    c2pnb163v3
    c2pnb176v1
    c2tnb191v1
    c2tnb191v2
    c2tnb191v3
    c2pnb208w1
    c2tnb239v1
    c2tnb239v2
    c2tnb239v3
    c2pnb272w1
    c2pnb304w1
    c2tnb359v1
    c2pnb368w1
    c2tnb431r1
    wap-wsg-idm-ecid-wtls1
    wap-wsg-idm-ecid-wtls3
    wap-wsg-idm-ecid-wtls4
    wap-wsg-idm-ecid-wtls5
    wap-wsg-idm-ecid-wtls10
    wap-wsg-idm-ecid-wtls11
);

my @explicit_only_curves = ();
push(@explicit_only_curves, qw(
        Oakley-EC2N-3
        Oakley-EC2N-4
    )) if !disabled("ec2m");

my @other_curves = ();
push(@other_curves, 'SM2')
    if !disabled("sm2");

my @curve_aliases = qw(
    P-192
    P-224
    P-256
    P-384
    P-521
);
push(@curve_aliases, qw(
    B-163
    B-233
    B-283
    B-409
    B-571
    K-163
    K-233
    K-283
    K-409
    K-571
)) if !disabled("ec2m");

my @curve_list = ();
push(@curve_list, @prime_curves);
push(@curve_list, @binary_curves)
    if !disabled("ec2m");
push(@curve_list, @other_curves);
push(@curve_list, @curve_aliases);

my %params_encodings =
    (
     'named_curve'      => \&supported_pass,
     'explicit'         => \&supported_pass
    );

my @output_formats = ('PEM', 'DER');

plan tests => scalar(@curve_list) * scalar(keys %params_encodings)
    * (1 + scalar(@output_formats)) # Try listed @output_formats and text output
    * 2                             # Test generating parameters and keys
    + 1                             # Checking that with no curve it fails
    + 1                             # Checking that with unknown curve it fails
    + 1                             # Subtest for explicit only curves
    + 1                             # base serializer test
    ;

ok(!run(app([ 'openssl', 'genpkey',
              '-algorithm', 'EC'])),
   "genpkey EC with no params should fail");

ok(!run(app([ 'openssl', 'genpkey',
              '-algorithm', 'EC',
              '-pkeyopt', 'ec_paramgen_curve:bogus_foobar_curve'])),
   "genpkey EC with unknown curve name should fail");

ok(run(app([ 'openssl', 'genpkey',
             '-provider-path', 'providers',
             '-provider', 'base',
             '-config', srctop_file("test", "default.cnf"),
             '-algorithm', 'EC',
             '-pkeyopt', 'ec_paramgen_curve:prime256v1',
             '-text'])),
    "generate a private key and serialize it using the base provider");

foreach my $curvename (@curve_list) {
    foreach my $paramenc (sort keys %params_encodings) {
        my $fn = $params_encodings{$paramenc};

        # --- Test generating parameters ---

        $fn->("genpkey EC params ${curvename} with ec_param_enc:'${paramenc}' (text)",
              app([ 'openssl', 'genpkey', '-genparam',
                    '-algorithm', 'EC',
                    '-pkeyopt', 'ec_paramgen_curve:'.$curvename,
                    '-pkeyopt', 'ec_param_enc:'.$paramenc,
                    '-text']));

        foreach my $outform (@output_formats) {
            my $outfile = "ecgen.${curvename}.${paramenc}." . lc $outform;
            $fn->("genpkey EC params ${curvename} with ec_param_enc:'${paramenc}' (${outform})",
                  app([ 'openssl', 'genpkey', '-genparam',
                        '-algorithm', 'EC',
                        '-pkeyopt', 'ec_paramgen_curve:'.$curvename,
                        '-pkeyopt', 'ec_param_enc:'.$paramenc,
                        '-outform', $outform,
                        '-out', $outfile]));
        }

        # --- Test generating actual keys ---

        $fn->("genpkey EC key on ${curvename} with ec_param_enc:'${paramenc}' (text)",
              app([ 'openssl', 'genpkey',
                    '-algorithm', 'EC',
                    '-pkeyopt', 'ec_paramgen_curve:'.$curvename,
                    '-pkeyopt', 'ec_param_enc:'.$paramenc,
                    '-text']));

        foreach my $outform (@output_formats) {
            my $outfile = "ecgen.${curvename}.${paramenc}." . lc $outform;
            $fn->("genpkey EC key on ${curvename} with ec_param_enc:'${paramenc}' (${outform})",
                  app([ 'openssl', 'genpkey',
                        '-algorithm', 'EC',
                        '-pkeyopt', 'ec_paramgen_curve:'.$curvename,
                        '-pkeyopt', 'ec_param_enc:'.$paramenc,
                        '-outform', $outform,
                        '-out', $outfile]));
        }
    }
}

subtest "test curves that only support explicit parameters encoding" => sub {
    plan skip_all => "This test is unsupported under current configuration"
            if scalar(@explicit_only_curves) <= 0;

    plan tests => scalar(@explicit_only_curves) * scalar(keys %params_encodings)
        * (1 + scalar(@output_formats)) # Try listed @output_formats and text output
        * 2                             # Test generating parameters and keys
        ;

    my %params_encodings =
        (
         'named_curve'      => \&supported_fail,
         'explicit'         => \&supported_pass
        );

    foreach my $curvename (@explicit_only_curves) {
        foreach my $paramenc (sort keys %params_encodings) {
            my $fn = $params_encodings{$paramenc};

            # --- Test generating parameters ---

            $fn->("genpkey EC params ${curvename} with ec_param_enc:'${paramenc}' (text)",
                  app([ 'openssl', 'genpkey', '-genparam',
                        '-algorithm', 'EC',
                        '-pkeyopt', 'ec_paramgen_curve:'.$curvename,
                        '-pkeyopt', 'ec_param_enc:'.$paramenc,
                        '-text']));

            foreach my $outform (@output_formats) {
                my $outfile = "ecgen.${curvename}.${paramenc}." . lc $outform;
                $fn->("genpkey EC params ${curvename} with ec_param_enc:'${paramenc}' (${outform})",
                      app([ 'openssl', 'genpkey', '-genparam',
                            '-algorithm', 'EC',
                            '-pkeyopt', 'ec_paramgen_curve:'.$curvename,
                            '-pkeyopt', 'ec_param_enc:'.$paramenc,
                            '-outform', $outform,
                            '-out', $outfile]));
            }

            # --- Test generating actual keys ---

            $fn->("genpkey EC key on ${curvename} with ec_param_enc:'${paramenc}' (text)",
                  app([ 'openssl', 'genpkey',
                        '-algorithm', 'EC',
                        '-pkeyopt', 'ec_paramgen_curve:'.$curvename,
                        '-pkeyopt', 'ec_param_enc:'.$paramenc,
                        '-text']));

            foreach my $outform (@output_formats) {
                my $outfile = "ecgen.${curvename}.${paramenc}." . lc $outform;
                $fn->("genpkey EC key on ${curvename} with ec_param_enc:'${paramenc}' (${outform})",
                      app([ 'openssl', 'genpkey',
                            '-algorithm', 'EC',
                            '-pkeyopt', 'ec_paramgen_curve:'.$curvename,
                            '-pkeyopt', 'ec_param_enc:'.$paramenc,
                            '-outform', $outform,
                            '-out', $outfile]));
            }
        }
    }
};
