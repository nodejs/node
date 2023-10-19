#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use OpenSSL::Test qw(:DEFAULT bldtop_dir srctop_file srctop_dir bldtop_file);
use OpenSSL::Test::Utils;

BEGIN {
setup("test_evp_fetch_prov");
}

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);

my @types = ( "digest", "cipher" );

my @testdata = (
    { config    => srctop_file("test", "default.cnf"),
      providers => [ 'default' ],
      tests  => [ { providers => [] },
                  { },
                  { args      => [ '-property', 'provider=default' ],
                    message   => 'using property "provider=default"' },
                  { args      => [ '-property', 'provider!=fips' ],
                    message   => 'using property "provider!=fips"' },
                  { args      => [ '-property', 'provider!=default', '-fetchfail' ],
                    message   =>
                        'using property "provider!=default" is expected to fail' },
                  { args      => [ '-property', 'provider=fips', '-fetchfail' ],
                    message   =>
                        'using property "provider=fips" is expected to fail' } ] }
);

unless ($no_fips) {
    push @testdata, (
        { config    => srctop_file("test", "fips.cnf"),
          providers => [ 'fips' ],
          tests     => [
              { args    => [ '-property', '' ] },
              { args    => [ '-property', 'provider=fips' ],
                message => 'using property "provider=fips"' },
              { args    => [ '-property', 'provider!=default' ],
                message => 'using property "provider!=default"' },
              { args      => [ '-property', 'provider=default', '-fetchfail' ],
                message   =>
                    'using property "provider=default" is expected to fail' },
              { args      => [ '-property', 'provider!=fips', '-fetchfail' ],
                message   =>
                    'using property "provider!=fips" is expected to fail' },
              { args    => [ '-property', 'fips=yes' ],
                message => 'using property "fips=yes"' },
              { args    => [ '-property', 'fips!=no' ],
                message => 'using property "fips!=no"' },
              { args    => [ '-property', '-fips' ],
                message => 'using property "-fips"' },
              { args    => [ '-property', 'fips=no', '-fetchfail' ],
                message => 'using property "fips=no is expected to fail"' },
              { args    => [ '-property', 'fips!=yes', '-fetchfail' ],
                message => 'using property "fips!=yes is expected to fail"' } ] },
        { config    => srctop_file("test", "default-and-fips.cnf"),
          providers => [ 'default', 'fips' ],
          tests     => [
              { args    => [ '-property', '' ] },
              { args      => [ '-property', 'provider!=default' ],
                message   => 'using property "provider!=default"' },
              { args      => [ '-property', 'provider=default' ],
                message   => 'using property "provider=default"' },
              { args      => [ '-property', 'provider!=fips' ],
                message   => 'using property "provider!=fips"' },
              { args      => [ '-property', 'provider=fips' ],
                message   => 'using property "provider=fips"' },
              { args    => [ '-property', 'fips=yes' ],
                message => 'using property "fips=yes"' },
              { args    => [ '-property', 'fips!=no' ],
                message => 'using property "fips!=no"' },
              { args    => [ '-property', '-fips' ],
                message => 'using property "-fips"' },
              { args    => [ '-property', 'fips=no' ],
                message => 'using property "fips=no"' },
              { args    => [ '-property', 'fips!=yes' ],
                message => 'using property "fips!=yes"' } ] },
    );
}

my $testcount = 0;
foreach (@testdata) {
    $testcount += scalar @{$_->{tests}};
}

plan tests => 1 + $testcount * scalar(@types);

ok(run(test(["evp_fetch_prov_test", "-defaultctx"])),
   "running evp_fetch_prov_test using the default libctx");

foreach my $alg (@types) {
    foreach my $testcase (@testdata) {
        $ENV{OPENSSL_CONF} = "";
        foreach my $test (@{$testcase->{tests}}) {
            my @testproviders =
                @{ $test->{providers} // $testcase->{providers} };
            my $testprovstr = @testproviders
                ? ' and loaded providers ' . join(' & ',
                                                  map { "'$_'" } @testproviders)
                : '';
            my @testargs = @{ $test->{args} // [] };
            my $testmsg =
                defined $test->{message} ? ' '.$test->{message} : '';

            my $message =
                "running evp_fetch_prov_test with $alg$testprovstr$testmsg";

            ok(run(test(["evp_fetch_prov_test", "-type", "$alg",
                         "-config", "$testcase->{config}",
                         @testargs, @testproviders])),
               $message);
        }
    }
}
