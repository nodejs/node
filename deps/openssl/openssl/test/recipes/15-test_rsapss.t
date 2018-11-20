#! /usr/bin/env perl
# Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT with srctop_file/;
use OpenSSL::Test::Utils;

setup("test_rsapss");

plan tests => 5;

#using test/testrsa.pem which happens to be a 512 bit RSA
ok(run(app(['openssl', 'dgst', '-sign', srctop_file('test', 'testrsa.pem'), '-sha1',
            '-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:-2',
            '-sigopt', 'rsa_mgf1_md:sha512', '-out', 'testrsapss.sig',
            srctop_file('test', 'testrsa.pem')])),
   "openssl dgst -sign");

with({ exit_checker => sub { return shift == 1; } },
     sub { ok(run(app(['openssl', 'dgst', '-sign', srctop_file('test', 'testrsa.pem'), '-sha512',
                       '-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:-2',
                       '-sigopt', 'rsa_mgf1_md:sha512', srctop_file('test', 'testrsa.pem')])),
              "openssl dgst -sign, expect to fail gracefully");
           ok(run(app(['openssl', 'dgst', '-sign', srctop_file('test', 'testrsa.pem'), '-sha512',
                       '-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:2147483647',
                       '-sigopt', 'rsa_mgf1_md:sha1', srctop_file('test', 'testrsa.pem')])),
              "openssl dgst -sign, expect to fail gracefully");
           ok(run(app(['openssl', 'dgst', '-prverify', srctop_file('test', 'testrsa.pem'), '-sha512',
                       '-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:-2',
                       '-sigopt', 'rsa_mgf1_md:sha512', '-signature', 'testrsapss.sig',
                       srctop_file('test', 'testrsa.pem')])),
              "openssl dgst -prverify, expect to fail gracefully");
         });

ok(run(app(['openssl', 'dgst', '-prverify', srctop_file('test', 'testrsa.pem'), '-sha1',
            '-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:-2',
            '-sigopt', 'rsa_mgf1_md:sha512', '-signature', 'testrsapss.sig',
            srctop_file('test', 'testrsa.pem')])),
   "openssl dgst -prverify");
unlink 'testrsapss.sig';
