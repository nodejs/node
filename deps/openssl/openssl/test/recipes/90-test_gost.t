#! /usr/bin/env perl
# Copyright 2018-2019 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_gost");

# The GOST ciphers are dynamically loaded via the GOST engine, so we must be
# able to support that. The engine also uses DSA, CMS and CMAC symbols, so we
# skip this test on no-dsa, no-cms or no-cmac.
plan skip_all => "GOST support is disabled in this OpenSSL build"
    if disabled("gost") || disabled("engine") || disabled("dynamic-engine")
       || disabled("dsa") || disabled("cms") || disabled("cmac");

plan skip_all => "TLSv1.3 or TLSv1.2 are disabled in this OpenSSL build"
    if disabled("tls1_3") || disabled("tls1_2");

plan skip_all => "No test GOST engine found"
    if !$ENV{OPENSSL_GOST_ENGINE_SO};

plan tests => 1;

$ENV{OPENSSL_CONF} = srctop_file("test", "recipes", "90-test_gost_data",
                                 "gost.conf");

ok(run(test(["gosttest",
             srctop_file("test", "recipes", "90-test_gost_data",
                         "server-cert2001.pem"),
             srctop_file("test", "recipes", "90-test_gost_data",
                         "server-key2001.pem"),
             srctop_file("test", "recipes", "90-test_gost_data",
                         "server-cert2012.pem"),
             srctop_file("test", "recipes", "90-test_gost_data",
                         "server-key2012.pem")])),
             "running gosttest");
