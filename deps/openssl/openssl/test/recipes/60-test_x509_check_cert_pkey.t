#! /usr/bin/env perl
# Copyright 2017-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test qw/:DEFAULT srctop_file/;
use OpenSSL::Test::Utils;

setup("test_x509_check_cert_pkey");

plan tests => 11;

sub src_file {
    return srctop_file("test", "certs", shift);
}

sub test_PEM_X509_INFO_read {
    my $file = shift;
    my $num = shift;
    ok(run(test(["x509_check_cert_pkey_test", src_file($file), $num])),
        "test_PEM_X509_INFO_read $file");
}

# rsa
ok(run(test(["x509_check_cert_pkey_test",
             src_file("servercert.pem"),
             src_file("serverkey.pem"), "cert", "ok"])));
# mismatched rsa
ok(run(test(["x509_check_cert_pkey_test",
             src_file("servercert.pem"),
             src_file("wrongkey.pem"), "cert", "failed"])));
SKIP: {
    skip "DSA disabled", 1, if disabled("dsa");
    # dsa
    ok(run(test(["x509_check_cert_pkey_test",
                 src_file("server-dsa-cert.pem"),
                 src_file("server-dsa-key.pem"), "cert", "ok"])));
}
# ecc
SKIP: {
    skip "EC disabled", 2 if disabled("ec");
    ok(run(test(["x509_check_cert_pkey_test",
                 src_file("server-ecdsa-cert.pem"),
                 src_file("server-ecdsa-key.pem"), "cert", "ok"])));

    test_PEM_X509_INFO_read("ec_privkey_with_chain.pem", "5");

}
# certificate request (rsa)
ok(run(test(["x509_check_cert_pkey_test",
             src_file("x509-check.csr"),
             src_file("x509-check-key.pem"), "req", "ok"])));
# mismatched certificate request (rsa)
ok(run(test(["x509_check_cert_pkey_test",
             src_file("x509-check.csr"),
             src_file("wrongkey.pem"), "req", "failed"])));

test_PEM_X509_INFO_read("root-cert.pem", "1");
test_PEM_X509_INFO_read("root-key.pem", "1");
test_PEM_X509_INFO_read("key-pass-12345.pem", "1");
test_PEM_X509_INFO_read("cyrillic_crl.utf8", "1");
