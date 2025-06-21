#! /usr/bin/env perl
# Copyright 2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use Getopt::Long;

my $activate = 1;
my $conditional_errors = 1;
my $security_checks = 1;
my $mac_key;
my $module_name;
my $section_name = "fips_sect";

GetOptions("key=s"              => \$mac_key,
           "module=s"           => \$module_name,
           "section_name=s"     => \$section_name)
    or die "Error when getting command line arguments";

my $mac_keylen = length($mac_key);

use Digest::SHA qw(hmac_sha256_hex);
my $module_size = [ stat($module_name) ]->[7];

open my $fh, "<:raw", $module_name or die "Trying to open $module_name: $!";
read $fh, my $data, $module_size or die "Trying to read $module_name: $!";
close $fh;

# Calculate HMAC-SHA256 in hex, and split it into a list of two character
# chunks, and join the chunks with colons.
my @module_mac
    = ( uc(hmac_sha256_hex($data, pack("H$mac_keylen", $mac_key))) =~ m/../g );
my $module_mac = join(':', @module_mac);

print <<_____;
[$section_name]
activate = $activate
conditional-errors = $conditional_errors
security-checks = $security_checks
module-mac = $module_mac
_____
