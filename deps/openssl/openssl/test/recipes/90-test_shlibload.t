#! /usr/bin/env perl
# Copyright 2016-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test qw/:DEFAULT bldtop_dir/;
use OpenSSL::Test::Utils;

#Load configdata.pm

BEGIN {
    setup("test_shlibload");
}
use lib bldtop_dir('.');
use configdata;

plan skip_all => "Test only supported in a shared build" if disabled("shared");

plan tests => 4;

my $libcrypto_idx = $unified_info{rename}->{libcrypto} // "libcrypto";
my $libssl_idx = $unified_info{rename}->{libssl} // "libssl";
my $libcrypto =
    $unified_info{sharednames}->{$libcrypto_idx}.$target{shared_extension_simple};
my $libssl =
    $unified_info{sharednames}->{$libssl_idx}.$target{shared_extension_simple};

ok(run(test(["shlibloadtest", "-crypto_first", $libcrypto, $libssl])),
   "running shlibloadtest -crypto_first");
ok(run(test(["shlibloadtest", "-ssl_first", $libcrypto, $libssl])),
   "running shlibloadtest -ssl_first");
ok(run(test(["shlibloadtest", "-just_crypto", $libcrypto, $libssl])),
   "running shlibloadtest -just_crypto");
ok(run(test(["shlibloadtest", "-dso_ref", $libcrypto, $libssl])),
   "running shlibloadtest -dso_ref");

