#! /usr/bin/env perl
# Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
# Copyright (c) 2017, Oracle and/or its affiliates.  All rights reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_x509_dup_cert");

plan tests => 1;

ok(run(test(["x509_dup_cert_test", srctop_file("test", "certs", "leaf.pem")])));
