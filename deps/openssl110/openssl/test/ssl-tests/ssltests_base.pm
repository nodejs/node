# -*- mode: perl; -*-
# Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

## SSL test configurations

package ssltests;

my $dir_sep = $^O ne "VMS" ? "/" : "";

our %base_server = (
    "Certificate" => "\${ENV::TEST_CERTS_DIR}${dir_sep}servercert.pem",
    "PrivateKey"  => "\${ENV::TEST_CERTS_DIR}${dir_sep}serverkey.pem",
    "CipherString" => "DEFAULT",
);

our %base_client = (
    "VerifyCAFile" => "\${ENV::TEST_CERTS_DIR}${dir_sep}rootcert.pem",
    "VerifyMode" => "Peer",
    "CipherString" => "DEFAULT",
);
