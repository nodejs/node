#! /usr/bin/env perl
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;
use OpenSSL::Test;

setup("test_ui");

plan tests => 1;

note <<"EOF";
The best way to test the UI interface is currently by using an openssl
command that uses password_callback.  The only one that does this is
'genrsa'.
Since password_callback uses a UI method derived from UI_OpenSSL(), it
ensures that one gets tested well enough as well.
EOF

my $outfile = "rsa_$$.pem";
ok(run(app(["openssl", "genrsa", "-passout", "pass:password", "-aes128",
            "-out", $outfile])),
   "Checking that genrsa with a password works properly");

unlink $outfile;
