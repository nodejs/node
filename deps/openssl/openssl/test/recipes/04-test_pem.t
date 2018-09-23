#! /usr/bin/env perl
# Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html
#
# ======================================================================


use strict;
use warnings;

use File::Compare qw/compare_text/;
use File::Basename;
use OpenSSL::Test qw/:DEFAULT srctop_file data_file/;
use OpenSSL::Test::Utils;

setup("test_pem_reading");

my $testsrc = srctop_file("test", "recipes", basename($0));

my $cmd = "openssl";

# map input PEM file to 1 if it should be accepted; 0 when should be rejected
my %cert_expected = (
    "cert-1023line.pem" => 1,
    "cert-1024line.pem" => 1,
    "cert-1025line.pem" => 1,
    "cert-255line.pem" => 1,
    "cert-256line.pem" => 1,
    "cert-257line.pem" => 1,
    "cert-blankline.pem" => 0,
    "cert-comment.pem" => 0,
    "cert-earlypad.pem" => 0,
    "cert-extrapad.pem" => 0,
    "cert-infixwhitespace.pem" => 1,
    "cert-junk.pem" => 0,
    "cert-leadingwhitespace.pem" => 1,
    "cert-longline.pem" => 1,
    "cert-misalignedpad.pem" => 0,
    "cert-onecolumn.pem" => 1,
    "cert-oneline.pem" => 1,
    "cert-shortandlongline.pem" => 1,
    "cert-shortline.pem" => 1,
    "cert-threecolumn.pem" => 1,
    "cert-trailingwhitespace.pem" => 1,
    "cert.pem" => 1
);
my %dsa_expected = (
    "dsa-1023line.pem" => 0,
    "dsa-1024line.pem" => 0,
    "dsa-1025line.pem" => 0,
    "dsa-255line.pem" => 0,
    "dsa-256line.pem" => 0,
    "dsa-257line.pem" => 0,
    "dsa-blankline.pem" => 0,
    "dsa-comment.pem" => 0,
    "dsa-corruptedheader.pem" => 0,
    "dsa-corruptiv.pem" => 0,
    "dsa-earlypad.pem" => 0,
    "dsa-extrapad.pem" => 0,
    "dsa-infixwhitespace.pem" => 0,
    "dsa-junk.pem" => 0,
    "dsa-leadingwhitespace.pem" => 0,
    "dsa-longline.pem" => 0,
    "dsa-misalignedpad.pem" => 0,
    "dsa-onecolumn.pem" => 0,
    "dsa-oneline.pem" => 0,
    "dsa-onelineheader.pem" => 0,
    "dsa-shortandlongline.pem" => 0,
    "dsa-shortline.pem" => 0,
    "dsa-threecolumn.pem" => 0,
    "dsa-trailingwhitespace.pem" => 1,
    "dsa.pem" => 1
);

plan tests =>  scalar keys(%cert_expected) + scalar keys(%dsa_expected) + 1;

foreach my $input (keys %cert_expected) {
    my @common = ($cmd, "x509", "-text", "-noout", "-inform", "PEM", "-in");
    my @data = run(app([@common, data_file($input)], stderr => undef), capture => 1);
    my @match = grep /The Great State of Long-Winded Certificate Field Names Whereby to Increase the Output Size/, @data;
    is((scalar @match > 0 ? 1 : 0), $cert_expected{$input});
}
SKIP: {
    skip "DSA support disabled, skipping...", (scalar keys %dsa_expected) unless !disabled("dsa");
    foreach my $input (keys %dsa_expected) {
        my @common = ($cmd, "pkey", "-inform", "PEM", "-passin", "file:" . data_file("wellknown"), "-noout", "-text", "-in");
        my @data;
        {
            local $ENV{MSYS2_ARG_CONV_EXCL} = "file:";
            @data = run(app([@common, data_file($input)], stderr => undef), capture => 1);
        }
        my @match = grep /68:42:02:16:63:54:16:eb:06:5c:ab:06:72:3b:78:/, @data;
        is((scalar @match > 0 ? 1 : 0), $dsa_expected{$input});
    }
}
SKIP: {
    skip "RSA support disabled, skipping...", 1 unless !disabled("rsa");
    my @common = ($cmd, "pkey", "-inform", "PEM", "-noout", "-text", "-in");
    my @data = run(app([@common, data_file("beermug.pem")], stderr => undef), capture => 1);
    my @match = grep /00:a0:3a:21:14:5d:cd:b6:d5:a0:3e:49:23:c1:3a:/, @data;
    ok(scalar @match > 0 ? 1 : 0);
}
