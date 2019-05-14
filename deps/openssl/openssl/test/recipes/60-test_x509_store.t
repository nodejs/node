#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Copy;
use File::Spec::Functions qw/:DEFAULT canonpath/;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_x509_store");

#If "openssl rehash -help" fails it's most likely because we're on a platform
#that doesn't support the rehash command (e.g. Windows)
plan skip_all => "test_rehash is not available on this platform"
    unless run(app(["openssl", "rehash", "-help"]));

# We use 'openssl verify' for these tests, as it contains everything
# we need to conduct these tests.  The tests here are a subset of the
# ones found in 25-test_verify.t

sub verify {
    my ($cert, $purpose, $trustedpath, $untrusted, @opts) = @_;
    my @args = qw(openssl verify -auth_level 1 -purpose);
    my @path = qw(test certs);
    push(@args, "$purpose", @opts);
    push(@args, "-CApath", $trustedpath);
    for (@$untrusted) { push(@args, "-untrusted", srctop_file(@path, "$_.pem")) }
    push(@args, srctop_file(@path, "$cert.pem"));
    run(app([@args]));
}

plan tests => 3;

indir "60-test_x509_store" => sub {
    for (("root-cert")) {
        copy(srctop_file("test", "certs", "$_.pem"), curdir());
    }
    ok(run(app([qw(openssl rehash), curdir()])), "Rehashing");

    # Canonical success
    ok(verify("ee-cert", "sslserver", curdir(), ["ca-cert"], "-show_chain"),
       "verify ee-cert");

    # Failure because root cert not present in CApath
    ok(!verify("ca-root2", "any", curdir(), [], "-show_chain"));
}, create => 1, cleanup => 1;
