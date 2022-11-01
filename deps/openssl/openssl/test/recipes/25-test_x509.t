#! /usr/bin/env perl
# Copyright 2015-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test qw/:DEFAULT srctop_file/;

setup("test_x509");

plan tests => 16;

# Prevent MSys2 filename munging for arguments that look like file paths but
# aren't
$ENV{MSYS2_ARG_CONV_EXCL} = "/CN=";

require_ok(srctop_file('test','recipes','tconversion.pl'));

my $pem = srctop_file("test/certs", "cyrillic.pem");
my $out = "cyrillic.out";
my $msb = srctop_file("test/certs", "cyrillic.msb");
my $utf = srctop_file("test/certs", "cyrillic.utf8");

ok(run(app(["openssl", "x509", "-text", "-in", $pem, "-out", $out,
            "-nameopt", "esc_msb"])));
is(cmp_text($out, srctop_file("test/certs", "cyrillic.msb")),
   0, 'Comparing esc_msb output');
ok(run(app(["openssl", "x509", "-text", "-in", $pem, "-out", $out,
            "-nameopt", "utf8"])));
is(cmp_text($out, srctop_file("test/certs", "cyrillic.utf8")),
   0, 'Comparing utf8 output');
unlink $out;

subtest 'x509 -- x.509 v1 certificate' => sub {
    tconversion("x509", srctop_file("test","testx509.pem"));
};
subtest 'x509 -- first x.509 v3 certificate' => sub {
    tconversion("x509", srctop_file("test","v3-cert1.pem"));
};
subtest 'x509 -- second x.509 v3 certificate' => sub {
    tconversion("x509", srctop_file("test","v3-cert2.pem"));
};

subtest 'x509 -- pathlen' => sub {
    ok(run(test(["v3ext", srctop_file("test/certs", "pathlen.pem")])));
};

# extracts issuer from a -text formatted-output
sub get_issuer {
    my $f = shift(@_);
    my $issuer = "";
    open my $fh, $f or die;
    while (my $line = <$fh>) {
        if ($line =~ /Issuer:/) {
            $issuer = $line;
        }
    }
    close $fh;
    return $issuer;
}

# Tests for signing certs (broken in 1.1.1o)
my $a_key = "a-key.pem";
my $a_cert = "a-cert.pem";
my $a2_cert = "a2-cert.pem";
my $ca_key = "ca-key.pem";
my $ca_cert = "ca-cert.pem";
my $cnf = srctop_file('apps', 'openssl.cnf');

# Create cert A
ok(run(app(["openssl", "req", "-x509", "-newkey", "rsa:2048",
            "-config", $cnf,
            "-keyout", $a_key, "-out", $a_cert, "-days", "365",
            "-nodes", "-subj", "/CN=test.example.com"])));
# Create cert CA - note key size
ok(run(app(["openssl", "req", "-x509", "-newkey", "rsa:4096",
            "-config", $cnf,
            "-keyout", $ca_key, "-out", $ca_cert, "-days", "3650",
            "-nodes", "-subj", "/CN=ca.example.com"])));
# Sign cert A with CA (errors on 1.1.1o)
ok(run(app(["openssl", "x509", "-in", $a_cert, "-CA", $ca_cert,
            "-CAkey", $ca_key, "-set_serial", "1234567890",
            "-preserve_dates", "-sha256", "-text", "-out", $a2_cert])));
# verify issuer is CA
ok (get_issuer($a2_cert) =~ /CN = ca.example.com/);

# Tests for issue #16080 (fixed in 1.1.1o)
my $b_key = "b-key.pem";
my $b_csr = "b-cert.csr";
my $b_cert = "b-cert.pem";
# Create the CSR
ok(run(app(["openssl", "req", "-new", "-newkey", "rsa:4096",
            "-keyout", $b_key, "-out", $b_csr, "-nodes",
            "-config", $cnf,
            "-subj", "/CN=b.example.com"])));
# Sign it - position of "-text" matters!
ok(run(app(["openssl", "x509", "-req", "-text", "-CAcreateserial",
            "-CA", $ca_cert, "-CAkey", $ca_key,
            "-in", $b_csr, "-out", $b_cert])));
# Verify issuer is CA
ok(get_issuer($b_cert) =~ /CN = ca.example.com/);
