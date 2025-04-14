#! /usr/bin/env perl
# Copyright 2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

# This collects specific use cases, and tests our handling

use File::Spec::Functions;
use File::Copy;
use MIME::Base64;
use OpenSSL::Test qw(:DEFAULT srctop_file srctop_dir bldtop_file bldtop_dir
                     data_file);
use OpenSSL::Test::Utils;

my $test_name = "test_store_cases";
setup($test_name);

plan tests => 2;

my $stderr;

# The case of the garbage PKCS#12 DER file where a passphrase was
# prompted for.  That should not have happened.
$stderr = 'garbage-pkcs12.stderr.txt';
ok(!run(app(['openssl', 'storeutl', '-passin', 'pass:invalidapass',
             data_file('garbage-pkcs12.p12')],
            stderr => $stderr)),
   "checking that storeutl fails when given a garbage pkcs12 file");
open DATA, $stderr;
@match = grep /try_pkcs12:.*?:maybe wrong password$/, <DATA>;
close DATA;
ok(scalar @match > 0 ? 0 : 1,
   "checking that storeutl didn't ask for a passphrase");
