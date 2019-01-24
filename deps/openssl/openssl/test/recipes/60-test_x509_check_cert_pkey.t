#! /usr/bin/env perl
# Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

setup("test_x509_check_cert_pkey");

plan tests => 6;

# rsa
ok(run(test(["x509_check_cert_pkey_test",
             srctop_file("test", "certs", "servercert.pem"),
             srctop_file("test", "certs", "serverkey.pem"), "cert", "ok"])));
# mismatched rsa
ok(run(test(["x509_check_cert_pkey_test",
             srctop_file("test", "certs", "servercert.pem"),
             srctop_file("test", "certs", "wrongkey.pem"), "cert", "failed"])));
SKIP: {
    skip "DSA disabled", 1, if disabled("dsa");
    # dsa
    ok(run(test(["x509_check_cert_pkey_test",
		 srctop_file("test", "certs", "server-dsa-cert.pem"),
		 srctop_file("test", "certs", "server-dsa-key.pem"), "cert", "ok"])));
}
# ecc
SKIP: {
    skip "EC disabled", 1 if disabled("ec");
    ok(run(test(["x509_check_cert_pkey_test",
                 srctop_file("test", "certs", "server-ecdsa-cert.pem"),
                 srctop_file("test", "certs", "server-ecdsa-key.pem"), "cert", "ok"])));
}
# certificate request (rsa)
ok(run(test(["x509_check_cert_pkey_test",
             srctop_file("test", "certs", "x509-check.csr"),
             srctop_file("test", "certs", "x509-check-key.pem"), "req", "ok"])));
# mismatched certificate request (rsa)
ok(run(test(["x509_check_cert_pkey_test",
             srctop_file("test", "certs", "x509-check.csr"),
             srctop_file("test", "certs", "wrongkey.pem"), "req", "failed"])));
