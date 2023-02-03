#! /usr/bin/env perl
# Copyright 2020-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use File::Spec;
use File::Spec::Functions qw/curdir abs2rel/;
use File::Copy;
use OpenSSL::Glob;
use OpenSSL::Test qw/:DEFAULT srctop_dir bldtop_dir bldtop_file srctop_file data_file/;
use OpenSSL::Test::Utils;

BEGIN {
    setup("test_cli_fips");
}
use lib srctop_dir('Configurations');
use lib bldtop_dir('.');
use platform;

my $no_check = disabled("fips") || disabled('fips-securitychecks');
plan skip_all => "Test only supported in a fips build with security checks"
    if $no_check;
plan tests => 11;

my $fipsmodule = bldtop_file('providers', platform->dso('fips'));
my $fipsconf = srctop_file("test", "fips-and-base.cnf");
my $defaultconf = srctop_file("test", "default.cnf");
my $tbs_data = $fipsmodule;
my $bogus_data = $fipsconf;

$ENV{OPENSSL_CONF} = $fipsconf;

ok(run(app(['openssl', 'list', '-public-key-methods', '-verbose'])),
   "provider listing of public key methods");
ok(run(app(['openssl', 'list', '-public-key-algorithms', '-verbose'])),
   "provider listing of public key algorithms");
ok(run(app(['openssl', 'list', '-key-managers', '-verbose'])),
   "provider listing of keymanagers");
ok(run(app(['openssl', 'list', '-key-exchange-algorithms', '-verbose'])),
   "provider listing of key exchange algorithms");
ok(run(app(['openssl', 'list', '-kem-algorithms', '-verbose'])),
   "provider listing of key encapsulation algorithms");
ok(run(app(['openssl', 'list', '-signature-algorithms', '-verbose'])),
   "provider listing of signature algorithms");
ok(run(app(['openssl', 'list', '-asymcipher-algorithms', '-verbose'])),
   "provider listing of encryption algorithms");
ok(run(app(['openssl', 'list', '-key-managers', '-verbose', '-select', 'DSA' ])),
   "provider listing of one item in the keymanager");

sub pubfrompriv {
    my $prefix = shift;
    my $key = shift;
    my $pub_key = shift;
    my $type = shift;

    ok(run(app(['openssl', 'pkey',
                '-in', $key,
                '-pubout',
                '-out', $pub_key])),
        $prefix.': '."Create the public key with $type parameters");

}

my $tsignverify_count = 9;
sub tsignverify {
    my $prefix = shift;
    my $fips_key = shift;
    my $fips_pub_key = shift;
    my $nonfips_key = shift;
    my $nonfips_pub_key = shift;
    my $fips_sigfile = $prefix.'.fips.sig';
    my $nonfips_sigfile = $prefix.'.nonfips.sig';
    my $sigfile = '';
    my $testtext = '';

    $ENV{OPENSSL_CONF} = $fipsconf;

    $sigfile = $fips_sigfile;
    $testtext = $prefix.': '.
        'Sign something with a FIPS key';
    ok(run(app(['openssl', 'dgst', '-sha256',
                '-sign', $fips_key,
                '-out', $sigfile,
                $tbs_data])),
       $testtext);

    $testtext = $prefix.': '.
        'Verify something with a FIPS key';
    ok(run(app(['openssl', 'dgst', '-sha256',
                '-verify', $fips_pub_key,
                '-signature', $sigfile,
                $tbs_data])),
       $testtext);

    $testtext = $prefix.': '.
        'Verify a valid signature against the wrong data with a FIPS key'.
        ' (should fail)';
    ok(!run(app(['openssl', 'dgst', '-sha256',
                 '-verify', $fips_pub_key,
                 '-signature', $sigfile,
                 $bogus_data])),
       $testtext);

    $ENV{OPENSSL_CONF} = $defaultconf;

    $sigfile = $nonfips_sigfile;
    $testtext = $prefix.': '.
        'Sign something with a non-FIPS key'.
        ' with the default provider';
    ok(run(app(['openssl', 'dgst', '-sha256',
                '-sign', $nonfips_key,
                '-out', $sigfile,
                $tbs_data])),
       $testtext);

    $testtext = $prefix.': '.
        'Verify something with a non-FIPS key'.
        ' with the default provider';
    ok(run(app(['openssl', 'dgst', '-sha256',
                '-verify', $nonfips_pub_key,
                '-signature', $sigfile,
                $tbs_data])),
       $testtext);

    $ENV{OPENSSL_CONF} = $fipsconf;

    $testtext = $prefix.': '.
        'Sign something with a non-FIPS key'.
        ' (should fail)';
    ok(!run(app(['openssl', 'dgst', '-sha256',
                 '-sign', $nonfips_key,
                 '-out', $prefix.'.nonfips.fail.sig',
                 $tbs_data])),
       $testtext);

    $testtext = $prefix.': '.
        'Verify something with a non-FIPS key'.
        ' (should fail)';
    ok(!run(app(['openssl', 'dgst', '-sha256',
                 '-verify', $nonfips_pub_key,
                 '-signature', $sigfile,
                 $tbs_data])),
       $testtext);

    $testtext = $prefix.': '.
        'Verify something with a non-FIPS key'.
		' in FIPS mode but with a non-FIPS property query';
    ok(run(app(['openssl', 'dgst',
				'-provider', 'default',
				'-propquery', '?fips!=yes',
				'-sha256',
                '-verify', $nonfips_pub_key,
                '-signature', $sigfile,
                $tbs_data])),
       $testtext);

    $testtext = $prefix.': '.
        'Verify a valid signature against the wrong data with a non-FIPS key'.
        ' (should fail)';
    ok(!run(app(['openssl', 'dgst', '-sha256',
                 '-verify', $nonfips_pub_key,
                 '-signature', $sigfile,
                 $bogus_data])),
       $testtext);
}

SKIP : {
    skip "FIPS EC tests because of no ec in this build", 1
        if disabled("ec");

    subtest EC => sub {
        my $testtext_prefix = 'EC';
        my $a_fips_curve = 'prime256v1';
        my $fips_key = $testtext_prefix.'.fips.priv.pem';
        my $fips_pub_key = $testtext_prefix.'.fips.pub.pem';
        my $a_nonfips_curve = 'brainpoolP256r1';
        my $nonfips_key = $testtext_prefix.'.nonfips.priv.pem';
        my $nonfips_pub_key = $testtext_prefix.'.nonfips.pub.pem';
        my $testtext = '';
        my $curvename = '';

        plan tests => 5 + $tsignverify_count;

        $ENV{OPENSSL_CONF} = $defaultconf;
        $curvename = $a_nonfips_curve;
        $testtext = $testtext_prefix.': '.
            'Generate a key with a non-FIPS algorithm with the default provider';
        ok(run(app(['openssl', 'genpkey', '-algorithm', 'EC',
                    '-pkeyopt', 'ec_paramgen_curve:'.$curvename,
                    '-out', $nonfips_key])),
           $testtext);

        pubfrompriv($testtext_prefix, $nonfips_key, $nonfips_pub_key, "non-FIPS");

        $ENV{OPENSSL_CONF} = $fipsconf;

        $curvename = $a_fips_curve;
        $testtext = $testtext_prefix.': '.
            'Generate a key with a FIPS algorithm';
        ok(run(app(['openssl', 'genpkey', '-algorithm', 'EC',
                    '-pkeyopt', 'ec_paramgen_curve:'.$curvename,
                    '-out', $fips_key])),
           $testtext);

        pubfrompriv($testtext_prefix, $fips_key, $fips_pub_key, "FIPS");

        $curvename = $a_nonfips_curve;
        $testtext = $testtext_prefix.': '.
            'Generate a key with a non-FIPS algorithm'.
            ' (should fail)';
        ok(!run(app(['openssl', 'genpkey', '-algorithm', 'EC',
                     '-pkeyopt', 'ec_paramgen_curve:'.$curvename,
                     '-out', $testtext_prefix.'.'.$curvename.'.priv.pem'])),
           $testtext);

        tsignverify($testtext_prefix, $fips_key, $fips_pub_key, $nonfips_key,
                    $nonfips_pub_key);
    };
}

SKIP: {
    skip "FIPS RSA tests because of no rsa in this build", 1
        if disabled("rsa");

    subtest RSA => sub {
        my $testtext_prefix = 'RSA';
        my $fips_key = $testtext_prefix.'.fips.priv.pem';
        my $fips_pub_key = $testtext_prefix.'.fips.pub.pem';
        my $nonfips_key = $testtext_prefix.'.nonfips.priv.pem';
        my $nonfips_pub_key = $testtext_prefix.'.nonfips.pub.pem';
        my $testtext = '';

        plan tests => 5 + $tsignverify_count;

        $ENV{OPENSSL_CONF} = $defaultconf;
        $testtext = $testtext_prefix.': '.
            'Generate a key with a non-FIPS algorithm with the default provider';
        ok(run(app(['openssl', 'genpkey', '-algorithm', 'RSA',
                    '-pkeyopt', 'rsa_keygen_bits:512',
                    '-out', $nonfips_key])),
           $testtext);

        pubfrompriv($testtext_prefix, $nonfips_key, $nonfips_pub_key, "non-FIPS");

        $ENV{OPENSSL_CONF} = $fipsconf;

        $testtext = $testtext_prefix.': '.
            'Generate a key with a FIPS algorithm';
        ok(run(app(['openssl', 'genpkey', '-algorithm', 'RSA',
                    '-pkeyopt', 'rsa_keygen_bits:2048',
                    '-out', $fips_key])),
           $testtext);

        pubfrompriv($testtext_prefix, $fips_key, $fips_pub_key, "FIPS");

        $testtext = $testtext_prefix.': '.
            'Generate a key with a non-FIPS algorithm'.
            ' (should fail)';
        ok(!run(app(['openssl', 'genpkey', '-algorithm', 'RSA',
                    '-pkeyopt', 'rsa_keygen_bits:512',
                     '-out', $testtext_prefix.'.fail.priv.pem'])),
           $testtext);

        tsignverify($testtext_prefix, $fips_key, $fips_pub_key, $nonfips_key,
                    $nonfips_pub_key);
    };
}

SKIP : {
    skip "FIPS DSA tests because of no dsa in this build", 1
        if disabled("dsa");

    subtest DSA => sub {
        my $testtext_prefix = 'DSA';
        my $fips_key = $testtext_prefix.'.fips.priv.pem';
        my $fips_pub_key = $testtext_prefix.'.fips.pub.pem';
        my $nonfips_key = $testtext_prefix.'.nonfips.priv.pem';
        my $nonfips_pub_key = $testtext_prefix.'.nonfips.pub.pem';
        my $testtext = '';
        my $fips_param = $testtext_prefix.'.fips.param.pem';
        my $nonfips_param = $testtext_prefix.'.nonfips.param.pem';
        my $shortnonfips_param = $testtext_prefix.'.shortnonfips.param.pem';

        plan tests => 13 + $tsignverify_count;

        $ENV{OPENSSL_CONF} = $defaultconf;

        $testtext = $testtext_prefix.': '.
            'Generate non-FIPS params with the default provider';
        ok(run(app(['openssl', 'genpkey', '-genparam',
                    '-algorithm', 'DSA',
                    '-pkeyopt', 'type:fips186_2',
                    '-pkeyopt', 'dsa_paramgen_bits:512',
                    '-out', $nonfips_param])),
           $testtext);

        $ENV{OPENSSL_CONF} = $fipsconf;

        $testtext = $testtext_prefix.': '.
            'Generate FIPS params';
        ok(run(app(['openssl', 'genpkey', '-genparam',
                    '-algorithm', 'DSA',
                    '-pkeyopt', 'dsa_paramgen_bits:2048',
                    '-out', $fips_param])),
           $testtext);

        $testtext = $testtext_prefix.': '.
            'Generate non-FIPS params'.
            ' (should fail)';
        ok(!run(app(['openssl', 'genpkey', '-genparam',
                     '-algorithm', 'DSA',
                    '-pkeyopt', 'dsa_paramgen_bits:512',
                     '-out', $testtext_prefix.'.fail.param.pem'])),
           $testtext);

        $testtext = $testtext_prefix.': '.
            'Generate non-FIPS params using non-FIPS property query'.
            ' (dsaparam)';
        ok(run(app(['openssl', 'dsaparam', '-provider', 'default',
                    '-propquery', '?fips!=yes',
                    '-out', $shortnonfips_param, '1024'])),
            $testtext);

        $testtext = $testtext_prefix.': '.
            'Generate non-FIPS params using non-FIPS property query'.
            ' (genpkey)';
        ok(run(app(['openssl', 'genpkey', '-provider', 'default',
                    '-propquery', '?fips!=yes',
                    '-genparam', '-algorithm', 'DSA',
                    '-pkeyopt', 'dsa_paramgen_bits:512'])),
            $testtext);

        $ENV{OPENSSL_CONF} = $defaultconf;

        $testtext = $testtext_prefix.': '.
            'Generate a key with non-FIPS params with the default provider';
        ok(run(app(['openssl', 'genpkey',
                    '-paramfile', $nonfips_param,
                    '-pkeyopt', 'type:fips186_2',
                    '-out', $nonfips_key])),
           $testtext);

        pubfrompriv($testtext_prefix, $nonfips_key, $nonfips_pub_key, "non-FIPS");

        $ENV{OPENSSL_CONF} = $fipsconf;

        $testtext = $testtext_prefix.': '.
            'Generate a key with FIPS parameters';
        ok(run(app(['openssl', 'genpkey',
                    '-paramfile', $fips_param,
                    '-pkeyopt', 'type:fips186_4',
                    '-out', $fips_key])),
           $testtext);

        pubfrompriv($testtext_prefix, $fips_key, $fips_pub_key, "FIPS");

        $testtext = $testtext_prefix.': '.
            'Generate a key with non-FIPS parameters'.
            ' (should fail)';
        ok(!run(app(['openssl', 'genpkey',
                     '-paramfile', $nonfips_param,
                     '-pkeyopt', 'type:fips186_2',
                     '-out', $testtext_prefix.'.fail.priv.pem'])),
           $testtext);

        $testtext = $testtext_prefix.': '.
            'Generate a key with non-FIPS parameters using non-FIPS property'.
            ' query (dsaparam)';
        ok(run(app(['openssl', 'dsaparam', '-provider', 'default',
                    '-propquery', '?fips!=yes',
                    '-noout', '-genkey', '1024'])),
            $testtext);

        $testtext = $testtext_prefix.': '.
            'Generate a key with non-FIPS parameters using non-FIPS property'.
            ' query (gendsa)';
        ok(run(app(['openssl', 'gendsa', '-provider', 'default',
                    '-propquery', '?fips!=yes',
                    $shortnonfips_param])),
            $testtext);

        $testtext = $testtext_prefix.': '.
            'Generate a key with non-FIPS parameters using non-FIPS property'.
            ' query (genpkey)';
        ok(run(app(['openssl', 'genpkey', '-provider', 'default',
                    '-propquery', '?fips!=yes',
                    '-paramfile', $nonfips_param,
                    '-pkeyopt', 'type:fips186_2',
                    '-out', $testtext_prefix.'.fail.priv.pem'])),
            $testtext);

        tsignverify($testtext_prefix, $fips_key, $fips_pub_key, $nonfips_key,
                    $nonfips_pub_key);
    };
}
