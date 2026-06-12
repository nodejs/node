#! /usr/bin/env perl
# Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use OpenSSL::Test;
use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT data_file srctop_file bldtop_dir/;
use Cwd qw(abs_path);

setup("test_external_rpki-client-portable");

plan skip_all => "No external tests in this configuration"
    if disabled("external-tests");

plan tests => 1;

$RPKI_VERSION = "9.6";
$RPKI_SRC = "rpki-client-".$RPKI_VERSION;
$RPKI_SUFFIX = ".tar.gz";
$RPKI_TARBALL = $RPKI_SRC.$RPKI_SUFFIX;
$RPKI_BASE_URL = "https://cdn.openbsd.org/pub/OpenBSD/rpki-client/";

$ENV{OPENSSL_MODULES} = abs_path(bldtop_dir("providers"));
$ENV{OPENSSL_CONF} = abs_path(srctop_file("test", "default-and-legacy.cnf"));
$ENV{RPKI_DOWNLOAD_URL} = $RPKI_BASE_URL.$RPKI_TARBALL;
$ENV{RPKI_TARBALL} = $RPKI_TARBALL;
$ENV{RPKI_SRC} = $RPKI_SRC;

ok(run(cmd([data_file("rpki-client-portable.sh")])), "running rpki-client tests");
