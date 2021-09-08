#! /usr/bin/env perl
# Copyright 2017-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw(:DEFAULT data_file);
use File::Compare qw(compare_text);

setup('test_bio_prefix');

my %input_result = (
    'in1.txt'  => [ 'args1.pl', 'out1.txt' ],
    'in2.txt' => [ 'args2.pl', 'out2.txt' ],
);

plan tests => 2 * scalar(keys %input_result);

foreach (sort keys %input_result) {
  SKIP: {
      my $input_path = data_file($_);
      my $args_path = data_file($input_result{$_}->[0]);
      my $expected_path = data_file($input_result{$_}->[1]);
      my $result_path = "test_bio_prefix-$_-stdout";
      my @args = do $args_path;

      skip "Problem prefixing $_", 1
          unless ok(run(test([ 'bio_prefix_text', @args ],
                             stdin => $input_path, stdout => $result_path)),
                    "prefixing $_ with args " . join(' ', @args));
      is(compare_text($result_path, $expected_path, \&cmp_line), 0,
         "comparing the dump of $_ with $expected_path");
    }
}

sub cmp_line {
    return 0 if scalar @_ == 0;

    if (scalar @_ != 2) {
        diag "Lines to compare less than 2: ", scalar @_;
        return -1;
    }

    $_[0] =~ s|\R$||;
    $_[1] =~ s|\R$||;
    my $r = $_[0] cmp $_[1];

    diag "Lines differ:\n<: $_[0]\n>: $_[1]\n" unless $r == 0;
    return $r;
}
