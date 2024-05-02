#! /usr/bin/env perl
# Copyright 2018-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use OpenSSL::Test;
use OpenSSL::Test::Utils;
use Storable qw(dclone);

setup("test_mac");

my @mac_tests = (
    { cmd => [qw{openssl mac -digest SHA1 -macopt hexkey:000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F}],
      type => 'HMAC',
      input => unpack("H*", "Sample message for keylen=blocklen"),
      expected => '5FD596EE78D5553C8FF4E72D266DFD192366DA29',
      desc => 'HMAC SHA1' },
    { cmd => [qw{openssl mac -macopt digest:SHA1 -macopt hexkey:000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F}],
      type => 'HMAC',
      input => unpack("H*", "Sample message for keylen=blocklen"),
      expected => '5FD596EE78D5553C8FF4E72D266DFD192366DA29',
      desc => 'HMAC SHA1 via -macopt' },
   { cmd => [qw{openssl mac -cipher AES-256-GCM -macopt hexkey:4C973DBC7364621674F8B5B89E5C15511FCED9216490FB1C1A2CAA0FFE0407E5 -macopt hexiv:7AE8E2CA4EC500012E58495C}],
     type => 'GMAC',
     input => '68F2E77696CE7AE8E2CA4EC588E541002E58495C08000F101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F404142434445464748494A4B4C4D0007',
     expected => '00BDA1B7E87608BCBF470F12157F4C07',
     desc => 'GMAC' },
   { cmd => [qw{openssl mac -macopt cipher:AES-256-GCM -macopt hexkey:4C973DBC7364621674F8B5B89E5C15511FCED9216490FB1C1A2CAA0FFE0407E5 -macopt hexiv:7AE8E2CA4EC500012E58495C}],
     type => 'GMAC',
     input => '68F2E77696CE7AE8E2CA4EC588E541002E58495C08000F101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F404142434445464748494A4B4C4D0007',
     expected => '00BDA1B7E87608BCBF470F12157F4C07',
     desc => 'GMAC via -macopt' },
   { cmd => [qw{openssl mac -macopt hexkey:404142434445464748494A4B4C4D4E4F505152535455565758595A5B5C5D5E5F -macopt xof:0}],
     type => 'KMAC128',
     input => '00010203',
     expected => 'E5780B0D3EA6F7D3A429C5706AA43A00FADBD7D49628839E3187243F456EE14E',
     desc => 'KMAC128' },
   { cmd => [qw{openssl mac -macopt hexkey:404142434445464748494A4B4C4D4E4F505152535455565758595A5B5C5D5E5F -macopt }, 'custom:My Tagged Application'],
     type => 'KMAC256',
     input => '00010203',
     expected => '20C570C31346F703C9AC36C61C03CB64C3970D0CFC787E9B79599D273A68D2F7F69D4CC3DE9D104A351689F27CF6F5951F0103F33F4F24871024D9C27773A8DD',
     desc => 'KMAC256' },
   { cmd => [qw{openssl mac -macopt hexkey:404142434445464748494A4B4C4D4E4F505152535455565758595A5B5C5D5E5F -macopt xof:1 -macopt}, 'custom:My Tagged Application'],
     type => 'KMAC256',
     input => '000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F404142434445464748494A4B4C4D4E4F505152535455565758595A5B5C5D5E5F606162636465666768696A6B6C6D6E6F707172737475767778797A7B7C7D7E7F808182838485868788898A8B8C8D8E8F909192939495969798999A9B9C9D9E9FA0A1A2A3A4A5A6A7A8A9AAABACADAEAFB0B1B2B3B4B5B6B7B8B9BABBBCBDBEBFC0C1C2C3C4C5C6C7',
     expected => 'D5BE731C954ED7732846BB59DBE3A8E30F83E77A4BFF4459F2F1C2B4ECEBB8CE67BA01C62E8AB8578D2D499BD1BB276768781190020A306A97DE281DCC30305D',
     desc => 'KMAC256 with xof len of 64' },
);

my @siphash_tests = (
    { cmd => [qw{openssl mac -macopt hexkey:000102030405060708090A0B0C0D0E0F}],
      type => 'SipHash',
      input => '00',
      expected => 'da87c1d86b99af44347659119b22fc45',
      desc => 'SipHash No input' }
);

my @cmac_tests = (
    { cmd => [qw{openssl mac -cipher AES-256-CBC -macopt hexkey:0B122AC8F34ED1FE082A3625D157561454167AC145A10BBF77C6A70596D574F1}],
      type => 'CMAC',
      input => '498B53FDEC87EDCBF07097DCCDE93A084BAD7501A224E388DF349CE18959FE8485F8AD1537F0D896EA73BEDC7214713F',
      expected => 'F62C46329B41085625669BAF51DEA66A',
      desc => 'CMAC AES-256-CBC' },
    { cmd => [qw{openssl mac -macopt cipher:AES-256-CBC -macopt hexkey:0B122AC8F34ED1FE082A3625D157561454167AC145A10BBF77C6A70596D574F1}],
      type => 'CMAC',
      input => '498B53FDEC87EDCBF07097DCCDE93A084BAD7501A224E388DF349CE18959FE8485F8AD1537F0D896EA73BEDC7214713F',
      expected => 'F62C46329B41085625669BAF51DEA66A',
      desc => 'CMAC AES-256-CBC' },
);

my @poly1305_tests = (
    { cmd => [qw{openssl mac -macopt hexkey:02000000000000000000000000000000ffffffffffffffffffffffffffffffff}],
      type => 'Poly1305',
      input => '02000000000000000000000000000000',
      expected => '03000000000000000000000000000000',
      desc => 'Poly1305 (wrap 2^128)' },
);

push @mac_tests, @siphash_tests unless disabled("siphash");
push @mac_tests, @cmac_tests unless disabled("cmac");
push @mac_tests, @poly1305_tests unless disabled("poly1305");

my @mac_fail_tests = (
    { cmd => [qw{openssl mac}],
      type => 'KMAC128',
      input => '00',
      err => 'EVP_MAC_Init',
      desc => 'KMAC128 Fail no key' },
    { cmd => [qw{openssl mac -propquery unknown -macopt hexkey:404142434445464748494A4B4C4D4E4F505152535455565758595A5B5C5D5E5F}],
      type => 'KMAC128',
      input => '00',
      err => 'Invalid MAC name KMAC128',
      desc => 'KMAC128 Fail unknown property' },
    { cmd => [qw{openssl mac -cipher AES-128-CBC -macopt hexkey:00}],
      type => 'HMAC',
      input => '00',
      err => 'MAC parameter error',
      desc => 'HMAC given a cipher' },
);

my @siphash_fail_tests = (
    { cmd => [qw{openssl mac}],
      type => 'SipHash',
      input => '00',
      err => '',
      desc => 'SipHash Fail no key' },
);

push @mac_fail_tests, @siphash_fail_tests unless disabled("siphash");

plan tests => (scalar @mac_tests * 2) + scalar @mac_fail_tests;

my $test_count = 0;

foreach (@mac_tests) {
    $test_count++;
    ok(compareline($_->{cmd}, $_->{type}, $_->{input}, $_->{expected}, $_->{err}), $_->{desc});
}
foreach (@mac_tests) {
    $test_count++;
    ok(comparefile($_->{cmd}, $_->{type}, $_->{input}, $_->{expected}), $_->{desc});
}

foreach (@mac_fail_tests) {
    $test_count++;
    ok(compareline($_->{cmd}, $_->{type}, $_->{input}, $_->{expected}, $_->{err}), $_->{desc});
}

# Create a temp input file and save the input data into it, and
# then compare the stdout output matches the expected value.
sub compareline {
    my $tmpfile = "input-$test_count.bin";
    my ($cmdarray_orig, $type, $input, $expect, $err) = @_;
    my $cmdarray = dclone $cmdarray_orig;
    if (defined($expect)) {
        $expect = uc $expect;
    }
    # Open a temporary input file and write $input to it
    open(my $in, '>', $tmpfile) or die "Could not open file";
    binmode($in);
    my $bin = pack("H*", $input);
    print $in $bin;
    close $in;

    # The last cmd parameter is the temporary input file we just created.
    my @other = ('-in', $tmpfile, $type);
    push @$cmdarray, @other;

    my @lines = run(app($cmdarray), capture => 1);
    # Not unlinking $tmpfile

    if (defined($expect)) {
        if ($lines[0] =~ m|^\Q${expect}\E\R$|) {
            return 1;
        } else {
            print "Got: $lines[0]";
            print "Exp: $expect\n";
            return 0;
        }
    }
    if (defined($err)) {
        if (defined($lines[0])) {
            $lines[0] =~ s/\s+$//;
            if ($lines[0] eq $err) {
                return 1;
            } else {
                print "Got: $lines[0]";
                print "Exp: $err\n";
                return 0;
            }
        } else {
            # expected an error
            return 1;
        }
    }
    return 0;
}

# Create a temp input file and save the input data into it, and
# use the '-bin -out <file>' commandline options to save results out to a file.
# Read this file back in and check its output matches the expected value.
sub comparefile {
    my $tmpfile = "input-$test_count.bin";
    my $outfile = "output-$test_count.bin";
    my ($cmdarray, $type, $input, $expect) = @_;
    $expect = uc $expect;

    # Open a temporary input file and write $input to it
    open(my $in, '>', $tmpfile) or die "Could not open file";
    binmode($in);
    my $bin = pack("H*", $input);
    print $in $bin;
    close $in;

    my @other = ("-binary", "-in", $tmpfile, "-out", $outfile, $type);
    push @$cmdarray, @other;

    run(app($cmdarray));
    # Not unlinking $tmpfile

    open(my $out, '<', $outfile) or die "Could not open file";
    binmode($out);
    my $buffer;
    my $BUFSIZE = 1024;
    read($out, $buffer, $BUFSIZE) or die "unable to read";
    my $line = uc unpack("H*", $buffer);
    close($out);
    # Not unlinking $outfile

    if ($line eq $expect) {
        return 1;
    } else {
        print "Got: $line\n";
        print "Exp: $expect\n";
        return 0;
    }
}
