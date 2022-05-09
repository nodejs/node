#! /usr/bin/env perl
# Copyright 2018-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_cmsapi");

plan skip_all => "CMS is disabled in this build" if disabled("cms");

plan tests => 1;

ok(run(test(["cmsapitest", srctop_file("test", "certs", "servercert.pem"),
             srctop_file("test", "certs", "serverkey.pem"),
             srctop_file("test", "recipes", "80-test_cmsapi_data", "encryptedData.der")])),
             "running cmsapitest");
