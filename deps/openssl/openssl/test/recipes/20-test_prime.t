#! /usr/bin/env perl
# Copyright 2020-2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw(:DEFAULT data_file);
use OpenSSL::Test;

setup("test_prime");

plan tests => 10;

my $prime_file      = data_file("prime.txt");
my $composite_file  = data_file("composite.txt");
my $long_number_file = data_file("long_number.txt");
my $short_number_file = data_file("short_number.txt");
my $non_number_file  = data_file("non_number.txt");
my $hex_number_file  = data_file("hex_number.txt");
my $multiple_lines_file  = data_file("multiple_lines.txt");
my $empty_file  = data_file("empty.txt");


ok(run(app(["openssl", "prime", "-in", $prime_file])),
   "Run openssl prime with prime number -in file");

ok(run(app(["openssl", "prime", "-in", $composite_file])),
   "Run openssl prime with composite number -in file");

ok(run(app(["openssl", "prime", "-in", $long_number_file])),
   "Run openssl prime with long number -in file");

ok(run(app(["openssl", "prime", "-in", $short_number_file])),
   "Run openssl prime with a short number -in file");

ok(run(app(["openssl", "prime", "-in", $non_number_file])),
   "Run openssl prime with non number -in file");

ok(run(app(["openssl", "prime", "-in", "-hex", $hex_number_file])),
   "Run openssl prime with hex number -in file");

ok(run(app(["openssl", "prime", "-in", $multiple_lines_file])),
   "Run openssl prime with -in file with multiple lines");

ok(run(app(["openssl", "prime", "-in", $empty_file])),
   "Run openssl prime with an empty -in file");

ok(run(app(["openssl", "prime", "-in", $prime_file, $composite_file, $long_number_file, $multiple_lines_file])),
   "Run openssl prime with multiple -in files");

ok(run(app(["openssl", "prime", "-in", "does_not_exist.txt"])),
   "Run openssl prime with -in file that does not exist");