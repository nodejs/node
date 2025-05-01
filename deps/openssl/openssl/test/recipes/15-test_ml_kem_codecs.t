#! /usr/bin/env perl
# Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use File::Copy;
use File::Compare qw/compare_text compare/;
use IO::File;
use OpenSSL::Glob;
use OpenSSL::Test qw/:DEFAULT data_file srctop_file bldtop_dir/;
use OpenSSL::Test::Utils;

setup("test_ml_kem_codecs");

my @algs = qw(512 768 1024);
my @formats = qw(seed-priv priv-only seed-only oqskeypair bare-seed bare-priv);

plan skip_all => "ML-KEM isn't supported in this build"
    if disabled("ml-kem");

plan tests => @algs * (25 + 10 * @formats);
my $seed = join ("", map {sprintf "%02x", $_} (0..63));
my $weed = join ("", map {sprintf "%02x", $_} (1..64));
my $ikme = join ("", map {sprintf "%02x", $_} (0..31));

foreach my $alg (@algs) {
    my $pub = sprintf("pub-%s.pem", $alg);
    my %formats = map { ($_, sprintf("prv-%s-%s.pem", $alg, $_)) } @formats;

    # (1 + 6 * @formats) tests
    my $i = 0;
    my $in0 = data_file($pub);
    my $der0 = sprintf("pub-%s.%d.der", $alg, $i++);
    ok(run(app(['openssl', 'pkey', '-pubin', '-in', $in0,
                '-outform', 'DER', '-out', $der0])));
    foreach my $f (keys %formats) {
        my $k = $formats{$f};
        my %pruned = %formats;
        delete $pruned{$f};
        my $rest = join(", ", keys %pruned);
        my $in = data_file($k);
        my $der = sprintf("pub-%s.%d.der", $alg, $i);
        #
        # Compare expected DER public key with DER public key of private
        ok(run(app(['openssl', 'pkey', '-in', $in, '-pubout',
                    '-outform', 'DER', '-out', $der])));
        ok(!compare($der0, $der),
            sprintf("pubkey DER match: %s, %s", $alg, $f));
        #
        # Compare expected PEM private key with regenerated key
        my $pem = sprintf("prv-%s-%s.%d.pem", $alg, $f, $i++);
        ok(run(app(['openssl', 'genpkey', '-out', $pem,
                    '-pkeyopt', "hexseed:$seed", '-algorithm', "ml-kem-$alg",
                    '-provparam', "ml-kem.output_formats=$f"])));
        ok(!compare_text($in, $pem),
            sprintf("prvkey PEM match: %s, %s", $alg, $f));

        ok(run(app(['openssl', 'pkey', '-in', $in, '-noout',
                     '-provparam', "ml-kem.input_formats=$f"])));
        ok(!run(app(['openssl', 'pkey', '-in', $in, '-noout',
                     '-provparam', "ml-kem.input_formats=$rest"])));
    }

    # (3 + 2 * @formats) tests
    # Check encap/decap ciphertext and shared secrets
    $i = 0;
    my $refct = sprintf("ct-%s.dat", $alg);
    my $refss = sprintf("ss-%s.dat", $alg);
    my $ct = sprintf("ct-%s.%d.dat", $alg, $i);
    my $ss0 = sprintf("ss-%s.%d.dat", $alg, $i++);
    ok(run(app(['openssl', 'pkeyutl', '-encap', '-inkey', $in0,
                '-pkeyopt', "hexikme:$ikme", '-secret',
                $ss0, '-out', $ct])));
    ok(!compare($ct, data_file($refct)),
        sprintf("reference ciphertext match: %s", $pub));
    ok(!compare($ss0, data_file($refss)),
        sprintf("reference secret match: %s", $pub));
    while (my ($f, $k) = each %formats) {
        my $in = data_file($k);
        my $ss = sprintf("ss-%s.%d.dat", $alg, $i++);
        ok(run(app(['openssl', 'pkeyutl', '-decap', '-inkey', $in,
                    '-in', $ct, '-secret', $ss])));
        ok(!compare($ss0, $ss),
            sprintf("shared secret match: %s with %s", $alg, $f));
    }

    # 6 tests
    # Test keygen seed suppression via the command-line and config file.
    my $seedless = sprintf("seedless-%s.gen.cli.pem", $alg);
    ok(run(app(['openssl', 'genpkey', '-provparam', 'ml-kem.retain_seed=no',
                '-algorithm', "ml-kem-$alg", '-pkeyopt', "hexseed:$seed",
                '-out', $seedless])));
    ok(!compare_text(data_file($formats{'priv-only'}), $seedless),
        sprintf("seedless via cli key match: %s", $alg));
    {
        local $ENV{'OPENSSL_CONF'} = data_file("ml-kem.cnf");
        local $ENV{'RETAIN_SEED'} = "no";
        $seedless = sprintf("seedless-%s.gen.cnf.pem", $alg);
        ok(run(app(['openssl', 'genpkey',
                    '-algorithm', "ml-kem-$alg", '-pkeyopt', "hexseed:$seed",
                    '-out', $seedless])));
        ok(!compare_text(data_file($formats{'priv-only'}), $seedless),
            sprintf("seedless via config match: %s", $alg));

        my $seedfull = sprintf("seedfull-%s.gen.conf+cli.pem", $alg);
        ok(run(app(['openssl', 'genpkey', '-provparam', 'ml-kem.retain_seed=yes',
                    '-algorithm', "ml-kem-$alg", '-pkeyopt', "hexseed:$seed",
                    '-out', $seedfull])));
        ok(!compare_text(data_file($formats{'seed-priv'}), $seedfull),
            sprintf("seedfull via cli vs. conf key match: %s", $alg));
    }

    # 6 tests
    # Test decoder seed suppression via the config file and command-line.
    $seedless = sprintf("seedless-%s.dec.cli.pem", $alg);
    ok(run(app(['openssl', 'pkey', '-provparam', 'ml-kem.retain_seed=no',
                '-in', data_file($formats{'seed-only'}), '-out', $seedless])));
    ok(!compare_text(data_file($formats{'priv-only'}), $seedless),
        sprintf("seedless via provparam key match: %s", $alg));
    {
        local $ENV{'OPENSSL_CONF'} = data_file("ml-kem.cnf");
        local $ENV{'RETAIN_SEED'} = "no";
        $seedless = sprintf("seedless-%s.dec.cnf.pem", $alg);
        ok(run(app(['openssl', 'pkey',
                    '-in', data_file($formats{'seed-only'}), '-out', $seedless])));
        ok(!compare_text(data_file($formats{'priv-only'}), $seedless),
            sprintf("seedless via config match: %s", $alg));

        my $seedfull = sprintf("seedfull-%s.dec.conf+cli.pem", $alg);
        ok(run(app(['openssl', 'pkey', '-provparam', 'ml-kem.retain_seed=yes',
                    '-in', data_file($formats{'seed-only'}), '-out', $seedfull])));
        ok(!compare_text(data_file($formats{'seed-priv'}), $seedfull),
            sprintf("seedfull via cli vs. conf key match: %s", $alg));
    }

    # 2 tests
    # Test decoder seed non-preference via the command-line.
    my $privpref = sprintf("privpref-%s.dec.cli.pem", $alg);
    ok(run(app(['openssl', 'pkey', '-provparam', 'ml-kem.prefer_seed=no',
                '-in', data_file($formats{'seed-priv'}), '-out', $privpref])));
    ok(!compare_text(data_file($formats{'priv-only'}), $privpref),
        sprintf("seed non-preference via provparam key match: %s", $alg));

    # (2 * @formats) tests
    # Check text encoding
    while (my ($f, $k) = each %formats) {
        my $txt =  sprintf("prv-%s-%s.txt", $alg,
                            ($f =~ m{seed}) ? 'seed' : 'priv');
        my $out = sprintf("prv-%s-%s.txt", $alg, $f);
        ok(run(app(['openssl', 'pkey', '-in', data_file($k),
                    '-noout', '-text', '-out', $out])));
        ok(!compare_text(data_file($txt), $out),
            sprintf("text form private key: %s with %s", $alg, $f));
    }

    # (6 tests): Test import/load PCT failure
    my $real = sprintf('real-%s.der', $alg);
    my $fake = sprintf('fake-%s.der', $alg);
    my $mixt = sprintf('mixt-%s.der', $alg);
    my $mash = sprintf('mash-%s.der', $alg);
    my $slen = $alg * 3 / 2; # Secret vector |s|
    my $plen = $slen + 64;   # Public |t|, |rho| and hash
    my $zlen = 32;           # FO implicit reject seed
    ok(run(app([qw(openssl genpkey -algorithm), "ml-kem-$alg",
                qw(-provparam ml-kem.output_formats=seed-priv -pkeyopt),
                "hexseed:$seed", qw(-outform DER -out), $real])),
        sprintf("create real private key: %s", $alg));
    ok(run(app([qw(openssl genpkey -algorithm), "ml-kem-$alg",
                qw(-provparam ml-kem.output_formats=seed-priv -pkeyopt),
                "hexseed:$weed", qw(-outform DER -out), $fake])),
        sprintf("create fake private key: %s", $alg));
    my $realfh = IO::File->new($real, "<:raw");
    my $fakefh = IO::File->new($fake, "<:raw");
    local $/ = undef;
    my $realder = <$realfh>;
    my $fakeder = <$fakefh>;
    $realfh->close();
    $fakefh->close();
    #
    # - 20 bytes PKCS8 fixed overhead,
    # - 4 byte private key octet string tag + length
    # - 4 byte seed + key sequence tag + length
    #   - 2 byte seed tag + length
    #     - 64 byte seed
    #   - 4 byte key tag + length
    #     - |dk| 's' vector
    #     - |ek| public key ('t' vector || 'rho')
    #     - implicit rejection 'z' seed component
    #
    my $svec_off = 28 + (2 + 64) + 4;
    my $p8_len = $svec_off + $slen + $plen + $zlen;
    ok((length($realder) == $p8_len && length($fakeder) == $p8_len),
        sprintf("Got expected DER lengths of %s seed-priv key", $alg));
    my $mixtder = substr($realder, 0, $svec_off + $slen)
        . substr($fakeder, $svec_off + $slen, $plen)
        . substr($realder, $svec_off + $slen + $plen, $zlen);
    my $mixtfh = IO::File->new($mixt, ">:raw");
    print $mixtfh $mixtder;
    $mixtfh->close();
    ok(run(app([qw(openssl pkey -inform DER -noout -in), $real])),
        sprintf("accept valid keypair: %s", $alg));
    ok(!run(app([qw(openssl pkey -provparam ml-kem.prefer_seed=no),
                 qw(-inform DER -noout -in), $mixt])),
        sprintf("reject real private and fake public: %s", $alg));
    ok(run(app([qw(openssl pkey -provparam ml-kem.prefer_seed=no),
                 qw(-provparam ml-kem.import_pct_type=none),
                 qw(-inform DER -noout -in), $mixt])),
        sprintf("Absent PCT accept fake public: %s", $alg));
    # Mutate the first byte of the |s| vector
    my $mashder = $realder;
    substr($mashder, $svec_off, 1) =~ s{(.)}{chr(ord($1)^1)}es;
    my $mashfh = IO::File->new($mash, ">:raw");
    print $mashfh $mashder;
    $mashfh->close();
    ok(!run(app([qw(openssl pkey -inform DER -noout -in), $mash])),
        sprintf("reject real private and mutated public: %s", $alg));
}
