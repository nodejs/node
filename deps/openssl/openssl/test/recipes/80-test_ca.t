#! /usr/bin/env perl
# Copyright 2015-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use POSIX;
use File::Path 2.00 qw/rmtree/;
use OpenSSL::Test qw/:DEFAULT cmdstr data_file srctop_file/;
use OpenSSL::Test::Utils;
use Time::Local qw/timegm/;

setup("test_ca");

$ENV{OPENSSL} = cmdstr(app(["openssl"]), display => 1);

my $cnf = srctop_file("test","ca-and-certs.cnf");
my $std_openssl_cnf = '"'
    . srctop_file("apps", $^O eq "VMS" ? "openssl-vms.cnf" : "openssl.cnf")
    . '"';

rmtree("demoCA", { safe => 0 });

plan tests => 15;
 SKIP: {
     my $cakey = srctop_file("test", "certs", "ca-key.pem");
     $ENV{OPENSSL_CONFIG} = qq(-config "$cnf");
     skip "failed creating CA structure", 4
         if !ok(run(perlapp(["CA.pl","-newca",
                             "-extra-req", "-key $cakey"], stdin => undef)),
                'creating CA structure');

     my $eekey = srctop_file("test", "certs", "ee-key.pem");
     $ENV{OPENSSL_CONFIG} = qq(-config "$cnf");
     skip "failed creating new certificate request", 3
         if !ok(run(perlapp(["CA.pl","-newreq",
                             '-extra-req', "-outform DER -section userreq -key $eekey"])),
                'creating certificate request');
     $ENV{OPENSSL_CONFIG} = qq(-rand_serial -inform DER -config "$std_openssl_cnf");
     skip "failed to sign certificate request", 2
         if !is(yes(cmdstr(perlapp(["CA.pl", "-sign"]))), 0,
                'signing certificate request');

     ok(run(perlapp(["CA.pl", "-verify", "newcert.pem"])),
        'verifying new certificate');

     skip "CT not configured, can't use -precert", 1
         if disabled("ct");

     my $eekey2 = srctop_file("test", "certs", "ee-key-3072.pem");
     $ENV{OPENSSL_CONFIG} = qq(-config "$cnf");
     ok(run(perlapp(["CA.pl", "-precert", '-extra-req', "-section userreq -key $eekey2"], stderr => undef)),
        'creating new pre-certificate');
}

SKIP: {
    skip "SM2 is not supported by this OpenSSL build", 1
        if disabled("sm2");

    is(yes(cmdstr(app(["openssl", "ca", "-config",
                       $cnf,
                       "-in", srctop_file("test", "certs", "sm2-csr.pem"),
                       "-out", "sm2-test.crt",
                       "-sigopt", "distid:1234567812345678",
                       "-vfyopt", "distid:1234567812345678",
                       "-md", "sm3",
                       "-cert", srctop_file("test", "certs", "sm2-root.crt"),
                       "-keyfile", srctop_file("test", "certs", "sm2-root.key")]))),
       0,
       "Signing SM2 certificate request");
}

test_revoke('notimes', {
    should_succeed => 1,
});
test_revoke('lastupdate_invalid', {
    lastupdate     => '1234567890',
    should_succeed => 0,
});
test_revoke('lastupdate_utctime', {
    lastupdate     => '200901123456Z',
    should_succeed => 1,
});
test_revoke('lastupdate_generalizedtime', {
    lastupdate     => '20990901123456Z',
    should_succeed => 1,
});
test_revoke('nextupdate_invalid', {
    nextupdate     => '1234567890',
    should_succeed => 0,
});
test_revoke('nextupdate_utctime', {
    nextupdate     => '200901123456Z',
    should_succeed => 1,
});
test_revoke('nextupdate_generalizedtime', {
    nextupdate     => '20990901123456Z',
    should_succeed => 1,
});
test_revoke('both_utctime', {
    lastupdate     => '200901123456Z',
    nextupdate     => '200908123456Z',
    should_succeed => 1,
});
test_revoke('both_generalizedtime', {
    lastupdate     => '20990901123456Z',
    nextupdate     => '20990908123456Z',
    should_succeed => 1,
});

sub test_revoke {
    my ($filename, $opts) = @_;

    subtest "Revoke certificate and generate CRL: $filename" => sub {
        # Before Perl 5.12.0, the range of times Perl could represent was
        # limited by the size of time_t, so Time::Local was hamstrung by the
        # Y2038 problem
        # Perl 5.12.0 onwards use an internal time implementation with a
        # guaranteed >32-bit time range on all architectures, so the tests
        # involving post-2038 times won't fail provided we're running under
        # that version or newer
        plan skip_all =>
            'Perl >= 5.12.0 required to run certificate revocation tests'
            if $] < 5.012000;

        $ENV{CN2} = $filename;
        ok(
            run(app(['openssl',
                     'req',
                     '-config',  $cnf,
                     '-new',
                     '-key',     data_file('revoked.key'),
                     '-out',     "$filename-req.pem",
                     '-section', 'userreq',
            ])),
            'Generate CSR'
        );
        delete $ENV{CN2};

        ok(
            run(app(['openssl',
                     'ca',
                     '-batch',
                     '-config',  $cnf,
                     '-in',      "$filename-req.pem",
                     '-out',     "$filename-cert.pem",
            ])),
            'Sign CSR'
        );

        ok(
            run(app(['openssl',
                     'ca',
                     '-config', $cnf,
                     '-revoke', "$filename-cert.pem",
            ])),
            'Revoke certificate'
        );

        my @gencrl_opts;

        if (exists $opts->{lastupdate}) {
            push @gencrl_opts, '-crl_lastupdate', $opts->{lastupdate};
        }

        if (exists $opts->{nextupdate}) {
            push @gencrl_opts, '-crl_nextupdate', $opts->{nextupdate};
        }

        is(
            run(app(['openssl',
                     'ca',
                     '-config', $cnf,
                     '-gencrl',
                     '-out',    "$filename-crl.pem",
                     '-crlsec', '60',
                     @gencrl_opts,
            ])),
            $opts->{should_succeed},
            'Generate CRL'
        );
        my $crl_gentime = time;

        # The following tests only need to run if the CRL was supposed to be
        # generated:
        return unless $opts->{should_succeed};

        my $crl_lastupdate = crl_field("$filename-crl.pem", 'lastUpdate');
        if (exists $opts->{lastupdate}) {
            is(
                $crl_lastupdate,
                rfc5280_time($opts->{lastupdate}),
                'CRL lastUpdate field has expected value'
            );
        } else {
            diag("CRL lastUpdate:   $crl_lastupdate");
            diag("openssl run time: $crl_gentime");
            ok(
                # Is the CRL's lastUpdate time within a second of the time that
                # `openssl ca -gencrl` was executed?
                $crl_gentime - 1 <= $crl_lastupdate && $crl_lastupdate <= $crl_gentime + 1,
                'CRL lastUpdate field has (roughly) expected value'
            );
        }

        my $crl_nextupdate = crl_field("$filename-crl.pem", 'nextUpdate');
        if (exists $opts->{nextupdate}) {
            is(
                $crl_nextupdate,
                rfc5280_time($opts->{nextupdate}),
                'CRL nextUpdate field has expected value'
            );
        } else {
            diag("CRL nextUpdate:   $crl_nextupdate");
            diag("openssl run time: $crl_gentime");
            ok(
                # Is the CRL's lastUpdate time within a second of the time that
                # `openssl ca -gencrl` was executed, taking into account the use
                # of '-crlsec 60'?
                $crl_gentime + 59 <= $crl_nextupdate && $crl_nextupdate <= $crl_gentime + 61,
                'CRL nextUpdate field has (roughly) expected value'
            );
        }
    };
}

sub yes {
    my $cntr = 10;
    open(PIPE, "|-", join(" ",@_));
    local $SIG{PIPE} = "IGNORE";
    1 while $cntr-- > 0 && print PIPE "y\n";
    close PIPE;
    return 0;
}

# Get the value of the lastUpdate or nextUpdate field from a CRL
sub crl_field {
    my ($crl_path, $field_name) = @_;

    my @out = run(
        app(['openssl',
             'crl',
             '-in', $crl_path,
             '-noout',
             '-' . lc($field_name),
        ]),
        capture => 1,
        statusvar => \my $exit,
    );
    ok($exit, "CRL $field_name field retrieved");
    diag("CRL $field_name: $out[0]");

    $out[0] =~ s/^\Q$field_name\E=//;
    $out[0] =~ s/\n?//;
    my $time = human_time($out[0]);

    return $time;
}

# Converts human-readable ASN1_TIME_print() output to Unix time
sub human_time {
    my ($human) = @_;

    my ($mo, $d, $h, $m, $s, $y) = $human =~ /^([A-Za-z]{3})\s+(\d+) (\d{2}):(\d{2}):(\d{2}) (\d{4})/;

    my %months = (
        Jan => 0, Feb => 1, Mar => 2, Apr => 3, May => 4,  Jun => 5,
        Jul => 6, Aug => 7, Sep => 8, Oct => 9, Nov => 10, Dec => 11,
    );

    return timegm($s, $m, $h, $d, $months{$mo}, $y);
}

# Converts an RFC 5280 timestamp to Unix time
sub rfc5280_time {
    my ($asn1) = @_;

    my ($y, $mo, $d, $h, $m, $s) = $asn1 =~ /^(\d{2,4})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})Z$/;

    return timegm($s, $m, $h, $d, $mo - 1, $y);
}
