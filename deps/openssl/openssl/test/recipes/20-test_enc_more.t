#! /usr/bin/env perl
# Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
# Copyright (c) 2017, Oracle and/or its affiliates.  All rights reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec::Functions qw/catfile/;
use File::Copy;
use File::Compare qw/compare_text/;
use File::Basename;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_evp_more");

my $testsrc = srctop_file("test", "recipes", basename($0));

my $cipherlist = undef;
my $plaintext = catfile(".", "testdatafile");
my $fail = "";
my $cmd = "openssl";

my $ciphersstatus = undef;
my @ciphers =
    grep(! /wrap|^$|^[^-]/,
         (map { split /\s+/ }
          run(app([$cmd, "enc", "-ciphers"]),
              capture => 1, statusvar => \$ciphersstatus)));

plan tests => 2 + scalar @ciphers;

SKIP: {
    skip "Problems getting ciphers...", 1 + scalar(@ciphers)
        unless ok($ciphersstatus, "Running 'openssl enc -ciphers'");
    unless (ok(copy($testsrc, $plaintext), "Copying $testsrc to $plaintext")) {
        diag($!);
        skip "Not initialized, skipping...", scalar(@ciphers);
    }

    foreach my $cipher (@ciphers) {
        my $ciphername = substr $cipher, 1;
        my $cipherfile = "$plaintext.$ciphername.cipher";
        my $clearfile = "$plaintext.$ciphername.clear";
        my @common = ( $cmd, "enc", "$cipher", "-k", "test" );

        ok(run(app([@common, "-e", "-in", $plaintext, "-out", $cipherfile]))
           && compare_text($plaintext, $cipherfile) != 0
           && run(app([@common, "-d", "-in", $cipherfile, "-out", $clearfile]))
           && compare_text($plaintext, $clearfile) == 0
           , $ciphername);
        unlink $cipherfile, $clearfile;
    }
}

unlink $plaintext;
