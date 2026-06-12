#! /usr/bin/env perl
# Copyright 2021-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use POSIX;
use OpenSSL::Test qw/:DEFAULT data_file/;
use File::Copy;

setup('test_ca_internals');

my @updatedb_tests = (
    {
        description => 'updatedb called before the first certificate expires',
        filename => 'index.txt',
        copydb => 1,
        testdate => '990101000000Z',
        need64bit => 0,
        expirelist => [ ]
    },
    {
        description => 'updatedb called before Y2k',
        filename => 'index.txt',
        copydb => 0,
        testdate => '991201000000Z',
        need64bit => 0,
        expirelist => [ '1000' ]
    },
    {
        description => 'updatedb called after year 2020',
        filename => 'index.txt',
        copydb => 0,
        testdate => '211201000000Z',
        need64bit => 0,
        expirelist => [ '1001' ]
    },
    {
        description => 'updatedb called in year 2049 (last year with 2 digits)',
        filename => 'index.txt',
        copydb => 0,
        testdate => '491201000000Z',
        need64bit => 1,
        expirelist => [ '1002' ]
    },
    {
        description => 'updatedb called in year 2050 (first year with 4 digits) before the last certificate expires',
        filename => 'index.txt',
        copydb => 0,
        testdate => '20500101000000Z',
        need64bit => 1,
        expirelist => [ ]
    },
    {
        description => 'updatedb called after the last certificate expired',
        filename => 'index.txt',
        copydb => 0,
        testdate => '20501201000000Z',
        need64bit => 1,
        expirelist => [ '1003' ]
    },
    {
        description => 'updatedb called for the first time after the last certificate expired',
        filename => 'index.txt',
        copydb => 1,
        testdate => '20501201000000Z',
        need64bit => 1,
        expirelist => [ '1000',
                        '1001',
                        '1002',
                        '1003' ]
    }
);

my @unsupported_commands = (
    {
        command => 'unsupported'
    }
);

# every "test_updatedb" makes 3 checks
plan tests => 3 * scalar(@updatedb_tests) +
              1 * scalar(@unsupported_commands);


foreach my $test (@updatedb_tests) {
    test_updatedb($test);
}
foreach my $test (@unsupported_commands) {
    test_unsupported_commands($test);
}


################### subs to do tests per supported command ################

sub test_unsupported_commands {
    my ($opts) = @_;

    run(
        test(['ca_internals_test',
                $opts->{command}
        ]),
        capture => 0,
        statusvar => \my $exit
    );

    is($exit, 0, "command '".$opts->{command}."' completed without an error");
}

sub test_updatedb {
    my ($opts) = @_;
    my $amtexpectedexpired = scalar(@{$opts->{expirelist}});
    my @output;
    my $expirelistcorrect = 1;
    my $cert;
    my $amtexpired = 0;
    my $skipped = 0;

    if ($opts->{copydb}) {
        copy(data_file('index.txt'), 'index.txt');
    }

    @output = run(
        test(['ca_internals_test',
            "do_updatedb",
            $opts->{filename},
            $opts->{testdate},
            $opts->{need64bit}
        ]),
        capture => 1,
        statusvar => \my $exit
    );

    foreach my $tmp (@output) {
        ($cert) = $tmp =~ /^[\x20\x23]*[^0-9A-Fa-f]*([0-9A-Fa-f]+)=Expired/;
        if ($tmp =~ /^[\x20\x23]*skipping test/) {
            $skipped = 1;
        }
        if (defined($cert) && (length($cert) > 0)) {
            $amtexpired++;
            my $expirefound = 0;
            foreach my $expire (@{$opts->{expirelist}}) {
                if ($expire eq $cert) {
                    $expirefound = 1;
                }
            }
            if ($expirefound != 1) {
                $expirelistcorrect = 0;
            }
        }
    }

    if ($skipped) {
        $amtexpired = $amtexpectedexpired;
        $expirelistcorrect = 1;
    }
    is($exit, 1, "ca_internals_test: returned EXIT_FAILURE (".$opts->{description}.")");
    is($amtexpired, $amtexpectedexpired, "ca_internals_test: amount of expired certificates differs from expected amount (".$opts->{description}.")");
    is($expirelistcorrect, 1, "ca_internals_test: list of expired certificates differs from expected list (".$opts->{description}.")");
}
