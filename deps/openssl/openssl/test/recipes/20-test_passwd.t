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
use OpenSSL::Test::Utils;

setup("test_passwd");

plan tests => disabled("des") ? 4 : 6;

ok(compare1stline([qw{openssl passwd password}], '^.{13}\R$'),
   'crypt password with random salt') if !disabled("des");
ok(compare1stline([qw{openssl passwd -1 password}], '^\$1\$.{8}\$.{22}\R$'),
   'BSD style MD5 password with random salt');
ok(compare1stline([qw{openssl passwd -apr1 password}], '^\$apr1\$.{8}\$.{22}\R$'),
   'Apache style MD5 password with random salt');
ok(compare1stline([qw{openssl passwd -salt xx password}], '^xxj31ZMTZzkVA\R$'),
   'crypt password with salt xx') if !disabled("des");
ok(compare1stline([qw{openssl passwd -salt xxxxxxxx -1 password}], '^\$1\$xxxxxxxx\$UYCIxa628\.9qXjpQCjM4a\.\R$'),
   'BSD style MD5 password with salt xxxxxxxx');
ok(compare1stline([qw{openssl passwd -salt xxxxxxxx -apr1 password}], '^\$apr1\$xxxxxxxx\$dxHfLAsjHkDRmG83UXe8K0\R$'),
   'Apache style MD5 password with salt xxxxxxxx');


sub compare1stline {
    my ($cmdarray, $regexp) = @_;
    my @lines = run(app($cmdarray), capture => 1);

    return $lines[0] =~ m|$regexp|;
}
