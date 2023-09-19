#! /usr/bin/env perl
# Copyright 2017-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT with srctop_file data_file/;
use OpenSSL::Test::Utils;

setup("test_rsapss");

plan tests => 11;

#using test/testrsa.pem which happens to be a 512 bit RSA
ok(run(app(['openssl', 'dgst', '-sign', srctop_file('test', 'testrsa.pem'), '-sha1',
            '-sigopt', 'rsa_padding_mode:pss',
            '-sigopt', 'rsa_pss_saltlen:max',
            '-sigopt', 'rsa_mgf1_md:sha512',
            '-out', 'testrsapss-restricted.sig',
            srctop_file('test', 'testrsa.pem')])),
   "openssl dgst -sign [plain RSA key, PSS padding mode, PSS restrictions]");

ok(run(app(['openssl', 'dgst', '-sign', srctop_file('test', 'testrsa.pem'), '-sha1',
            '-sigopt', 'rsa_padding_mode:pss',
            '-out', 'testrsapss-unrestricted.sig',
            srctop_file('test', 'testrsa.pem')])),
   "openssl dgst -sign [plain RSA key, PSS padding mode, no PSS restrictions]");

ok(!run(app(['openssl', 'dgst', '-sign', srctop_file('test', 'testrsa.pem'), '-sha512',
             '-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:max',
             '-sigopt', 'rsa_mgf1_md:sha512', srctop_file('test', 'testrsa.pem')])),
   "openssl dgst -sign, expect to fail gracefully");

ok(!run(app(['openssl', 'dgst', '-sign', srctop_file('test', 'testrsa.pem'), '-sha512',
             '-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:2147483647',
             '-sigopt', 'rsa_mgf1_md:sha1', srctop_file('test', 'testrsa.pem')])),
   "openssl dgst -sign, expect to fail gracefully");

ok(!run(app(['openssl', 'dgst', '-prverify', srctop_file('test', 'testrsa.pem'), '-sha512',
             '-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:max',
             '-sigopt', 'rsa_mgf1_md:sha512', '-signature', 'testrsapss.sig',
             srctop_file('test', 'testrsa.pem')])),
   "openssl dgst -prverify, expect to fail gracefully");

ok(run(app(['openssl', 'dgst', '-prverify', srctop_file('test', 'testrsa.pem'),
            '-sha1',
            '-sigopt', 'rsa_padding_mode:pss',
            '-sigopt', 'rsa_pss_saltlen:max',
            '-sigopt', 'rsa_mgf1_md:sha512',
            '-signature', 'testrsapss-restricted.sig',
            srctop_file('test', 'testrsa.pem')])),
   "openssl dgst -prverify [plain RSA key, PSS padding mode, PSS restrictions]");

ok(run(app(['openssl', 'dgst', '-prverify', srctop_file('test', 'testrsa.pem'),
            '-sha1',
            '-sigopt', 'rsa_padding_mode:pss',
            '-sigopt', 'rsa_pss_saltlen:42',
            '-sigopt', 'rsa_mgf1_md:sha512',
            '-signature', 'testrsapss-restricted.sig',
            srctop_file('test', 'testrsa.pem')])),
   "openssl dgst -sign rsa512bit.pem -sha1 -sigopt rsa_pss_saltlen:max produces 42 bits of PSS salt");

ok(run(app(['openssl', 'dgst', '-prverify', srctop_file('test', 'testrsa.pem'),
            '-sha1',
            '-sigopt', 'rsa_padding_mode:pss',
            '-signature', 'testrsapss-unrestricted.sig',
            srctop_file('test', 'testrsa.pem')])),
   "openssl dgst -prverify [plain RSA key, PSS padding mode, no PSS restrictions]");

# Test that RSA-PSS keys are supported by genpkey and rsa commands.
{
   my $rsapss = "rsapss.key";
   ok(run(app(['openssl', 'genpkey', '-algorithm', 'RSA-PSS',
               '-pkeyopt', 'rsa_keygen_bits:1024',
               '--out', $rsapss])));
   ok(run(app(['openssl', 'rsa', '-check',
               '-in', $rsapss])));
}

ok(!run(app([ 'openssl', 'rsa',
             '-in' => data_file('negativesaltlen.pem')],
             '-out' => 'badout')));
