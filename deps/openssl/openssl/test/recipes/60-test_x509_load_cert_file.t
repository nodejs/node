#! /usr/bin/env perl
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_load_cert_file");

plan tests => 1;

ok(run(test(["x509_load_cert_file_test", srctop_file("test", "certs", "leaf-chain.pem"),
             srctop_file("test", "certs", "cyrillic_crl.pem")])));
