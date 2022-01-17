#! /usr/bin/env perl
# Copyright 2016-2021 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use File::Spec::Functions;
use File::Copy;
use MIME::Base64;
use OpenSSL::Test qw(:DEFAULT srctop_file srctop_dir bldtop_file bldtop_dir
                     data_file);
use OpenSSL::Test::Utils;

my $test_name = "test_store";
setup($test_name);

my $use_md5 = !disabled("md5");
my $use_des = !(disabled("des") || disabled("legacy")); # also affects 3des and pkcs12 app
my $use_dsa = !disabled("dsa");
my $use_ecc = !disabled("ec");

my @noexist_files =
    ( "test/blahdiblah.pem",
      "test/blahdibleh.der" );
my @src_files =
    ( "test/testx509.pem",
      "test/testrsa.pem",
      "test/testrsapub.pem",
      "test/testcrl.pem",
      "apps/server.pem" );
my @data_files =
    ( "testrsa.msb" );
push(@data_files,
     ( "testrsa.pvk" ))
    unless disabled("legacy") || disabled("rc4");
my @src_rsa_files =
    ( "test/testrsa.pem",
      "test/testrsapub.pem" );
my @generated_files =
    (
     ### generated from the source files

     "testx509.der",
     "testrsa.der",
     "testrsapub.der",
     "testcrl.der",

     ### generated locally

     "rsa-key-pkcs1.pem", "rsa-key-pkcs1.der",
     "rsa-key-pkcs1-aes128.pem",
     "rsa-key-pkcs8.pem", "rsa-key-pkcs8.der",
     "rsa-key-pkcs8-pbes2-sha1.pem", "rsa-key-pkcs8-pbes2-sha1.der",
     "rsa-key-pkcs8-pbes2-sha256.pem", "rsa-key-pkcs8-pbes2-sha256.der",
    );
push(@generated_files, (
     "rsa-key-pkcs8-pbes1-sha1-3des.pem", "rsa-key-pkcs8-pbes1-sha1-3des.der",
    )) if $use_des;
push(@generated_files, (
     "rsa-key-sha1-3des-sha1.p12", "rsa-key-sha1-3des-sha256.p12",
     "rsa-key-aes256-cbc-sha256.p12",
     "rsa-key-md5-des-sha1.p12",
     "rsa-key-aes256-cbc-md5-des-sha256.p12"
     )) if $use_des;
push(@generated_files, (
     "rsa-key-pkcs8-pbes1-md5-des.pem", "rsa-key-pkcs8-pbes1-md5-des.der"
     )) if $use_md5 && $use_des;
push(@generated_files, (
     "dsa-key-pkcs1.pem", "dsa-key-pkcs1.der",
     "dsa-key-pkcs1-aes128.pem",
     "dsa-key-pkcs8.pem", "dsa-key-pkcs8.der",
     "dsa-key-pkcs8-pbes2-sha1.pem", "dsa-key-pkcs8-pbes2-sha1.der",
     )) if $use_dsa;
push(@generated_files, "dsa-key-aes256-cbc-sha256.p12") if $use_dsa && $use_des;
push(@generated_files, (
     "ec-key-pkcs1.pem", "ec-key-pkcs1.der",
     "ec-key-pkcs1-aes128.pem",
     "ec-key-pkcs8.pem", "ec-key-pkcs8.der",
     "ec-key-pkcs8-pbes2-sha1.pem", "ec-key-pkcs8-pbes2-sha1.der",
     )) if $use_ecc;
push(@generated_files, "ec-key-aes256-cbc-sha256.p12") if $use_ecc && $use_des;
my %generated_file_files =
    $^O eq 'linux'
    ? ( "test/testx509.pem" => "file:testx509.pem",
        "test/testrsa.pem" => "file:testrsa.pem",
        "test/testrsapub.pem" => "file:testrsapub.pem",
        "test/testcrl.pem" => "file:testcrl.pem",
        "apps/server.pem" => "file:server.pem" )
    : ();
my @noexist_file_files =
    ( "file:blahdiblah.pem",
      "file:test/blahdibleh.der" );

# There is more than one method to get a 'file:' loader.
# The default is a built-in provider implementation.
# However, there is also an engine, specially for testing purposes.
#
# @methods is a collection of extra 'openssl storeutl' arguments used to
# try the different methods.
my @methods;
my @prov_method = qw(-provider default);
push @prov_method, qw(-provider legacy) unless disabled('legacy');
push @methods, [ @prov_method ];
push @methods, [qw(-engine loader_attic)]
    unless disabled('loadereng');

my $n = scalar @methods
    * ( (3 * scalar @noexist_files)
        + (6 * scalar @src_files)
        + (2 * scalar @data_files)
        + (4 * scalar @generated_files)
        + (scalar keys %generated_file_files)
        + (scalar @noexist_file_files)
        + 3
        + 11 );

# Test doesn't work under msys because the file name munging doesn't work
# correctly with the "ot:" prefix
my $do_test_ossltest_store =
    !(disabled("engine") || disabled("dynamic-engine") || $^O =~ /^msys$/);

if ($do_test_ossltest_store) {
    # test loading with apps 'org.openssl.engine:' loader, using the
    # ossltest engine.
    $n += 4 * scalar @src_rsa_files;
}

plan skip_all => "No plan" if $n == 0;

plan tests => $n;

indir "store_$$" => sub {
    if ($do_test_ossltest_store) {
        # ossltest loads PEM files, with names prefixed with 'ot:'.
        # This prefix ensures that the files are, in fact, loaded through
        # that engine and not mistakenly going through the 'file:' loader.

        my $engine_scheme = 'org.openssl.engine:';
        $ENV{OPENSSL_ENGINES} = bldtop_dir("engines");

        foreach (@src_rsa_files) {
            my $file = srctop_file($_);
            my $file_abs = to_abs_file($file);
            my @pubin = $_ =~ m|pub\.pem$| ? ("-pubin") : ();

            ok(run(app(["openssl", "rsa", "-text", "-noout", @pubin,
                        "-engine", "ossltest", "-inform", "engine",
                        "-in", "ot:$file"])));
            ok(run(app(["openssl", "rsa", "-text", "-noout", @pubin,
                        "-engine", "ossltest", "-inform", "engine",
                        "-in", "ot:$file_abs"])));
            ok(run(app(["openssl", "rsa", "-text", "-noout", @pubin,
                        "-in", "${engine_scheme}ossltest:ot:$file"])));
            ok(run(app(["openssl", "rsa", "-text", "-noout", @pubin,
                        "-in", "${engine_scheme}ossltest:ot:$file_abs"])));
        }
    }

 SKIP:
    {
        init() or die "init failed";

        my $rehash = init_rehash();

        foreach my $method (@methods) {
            my @storeutl = ( qw(openssl storeutl), @$method );

            foreach (@noexist_files) {
                my $file = srctop_file($_);

                ok(!run(app([@storeutl, "-noout", $file])));
                ok(!run(app([@storeutl, "-noout", to_abs_file($file)])));
                {
                    local $ENV{MSYS2_ARG_CONV_EXCL} = "file:";

                    ok(!run(app([@storeutl, "-noout",
                                 to_abs_file_uri($file)])));
                }
            }
            foreach (@src_files) {
                my $file = srctop_file($_);

                ok(run(app([@storeutl, "-noout", $file])));
                ok(run(app([@storeutl, "-noout", to_abs_file($file)])));
              SKIP:
                {
                    skip "file: tests disabled on MingW", 4  if $^O =~ /^msys$/;

                    ok(run(app([@storeutl, "-noout",
                                to_abs_file_uri($file)])));
                    ok(run(app([@storeutl, "-noout",
                                to_abs_file_uri($file, 0, "")])));
                    ok(run(app([@storeutl, "-noout",
                                to_abs_file_uri($file, 0, "localhost")])));
                    ok(!run(app([@storeutl, "-noout",
                                 to_abs_file_uri($file, 0, "dummy")])));
                }
            }
            foreach (@data_files) {
                my $file = data_file($_);

                ok(run(app([@storeutl, "-noout", "-passin", "pass:password",
                            $file])));
                ok(run(app([@storeutl, "-noout", "-passin", "pass:password",
                            to_abs_file($file)])));
            }
            foreach (@generated_files) {
                ok(run(app([@storeutl, "-noout", "-passin", "pass:password",
                            $_])));
                ok(run(app([@storeutl,  "-noout", "-passin", "pass:password",
                            to_abs_file($_)])));

              SKIP:
                {
                    skip "file: tests disabled on MingW", 2  if $^O =~ /^msys$/;

                    ok(run(app([@storeutl, "-noout", "-passin",
                                "pass:password", to_abs_file_uri($_)])));
                    ok(!run(app([@storeutl, "-noout", "-passin",
                                 "pass:password", to_file_uri($_)])));
                }
            }
            foreach (values %generated_file_files) {
              SKIP:
                {
                    skip "file: tests disabled on MingW", 1  if $^O =~ /^msys$/;

                    ok(run(app([@storeutl,  "-noout", $_])));
                }
            }
            foreach (@noexist_file_files) {
              SKIP:
                {
                    skip "file: tests disabled on MingW", 1  if $^O =~ /^msys$/;

                    ok(!run(app([@storeutl,  "-noout", $_])));
                }
            }
            {
                my $dir = srctop_dir("test", "certs");

                ok(run(app([@storeutl,  "-noout", $dir])));
                ok(run(app([@storeutl,  "-noout", to_abs_file($dir, 1)])));
              SKIP:
                {
                    skip "file: tests disabled on MingW", 1  if $^O =~ /^msys$/;

                    ok(run(app([@storeutl,  "-noout",
                                to_abs_file_uri($dir, 1)])));
                }
            }

            ok(!run(app([@storeutl, '-noout',
                         '-subject', '/C=AU/ST=QLD/CN=SSLeay\/rsa test cert',
                         srctop_file('test', 'testx509.pem')])),
               "Checking that -subject can't be used with a single file");

            ok(run(app([@storeutl, '-certs', '-noout',
                        srctop_file('test', 'testx509.pem')])),
               "Checking that -certs returns 1 object on a certificate file");
            ok(run(app([@storeutl, '-certs', '-noout',
                        srctop_file('test', 'testcrl.pem')])),
               "Checking that -certs returns 0 objects on a CRL file");

            ok(run(app([@storeutl, '-crls', '-noout',
                        srctop_file('test', 'testx509.pem')])),
               "Checking that -crls returns 0 objects on a certificate file");
            ok(run(app([@storeutl, '-crls', '-noout',
                        srctop_file('test', 'testcrl.pem')])),
               "Checking that -crls returns 1 object on a CRL file");

          SKIP: {
              skip "failed rehash initialisation", 6 unless $rehash;

              # subject from testx509.pem:
              # '/C=AU/ST=QLD/CN=SSLeay\/rsa test cert'
              # issuer from testcrl.pem:
              # '/C=US/O=RSA Data Security, Inc./OU=Secure Server Certification Authority'
              ok(run(app([@storeutl, '-noout',
                          '-subject', '/C=AU/ST=QLD/CN=SSLeay\/rsa test cert',
                          catdir(curdir(), 'rehash')])));
              ok(run(app([@storeutl, '-noout',
                          '-subject',
                          '/C=US/O=RSA Data Security, Inc./OU=Secure Server Certification Authority',
                          catdir(curdir(), 'rehash')])));
              ok(run(app([@storeutl, '-noout', '-certs',
                          '-subject', '/C=AU/ST=QLD/CN=SSLeay\/rsa test cert',
                          catdir(curdir(), 'rehash')])));
              ok(run(app([@storeutl, '-noout', '-crls',
                          '-subject', '/C=AU/ST=QLD/CN=SSLeay\/rsa test cert',
                          catdir(curdir(), 'rehash')])));
              ok(run(app([@storeutl, '-noout', '-certs',
                          '-subject',
                          '/C=US/O=RSA Data Security, Inc./OU=Secure Server Certification Authority',
                          catdir(curdir(), 'rehash')])));
              ok(run(app([@storeutl, '-noout', '-crls',
                          '-subject',
                          '/C=US/O=RSA Data Security, Inc./OU=Secure Server Certification Authority',
                          catdir(curdir(), 'rehash')])));
            }
        }
    }
}, create => 1, cleanup => 1;

sub init {
    my $cnf = srctop_file('test', 'ca-and-certs.cnf');
    my $cakey = srctop_file('test', 'certs', 'ca-key.pem');
    my @std_args = qw(-provider default);
    push @std_args, qw(-provider legacy)
        unless disabled('legacy');
    return (
            # rsa-key-pkcs1.pem
            run(app(["openssl", "pkey", @std_args,
                     "-in", data_file("rsa-key-2432.pem"),
                     "-out", "rsa-key-pkcs1.pem"]))
            # rsa-key-pkcs1-aes128.pem
            && run(app(["openssl", "rsa", @std_args,
                        "-passout", "pass:password", "-aes128",
                        "-in", "rsa-key-pkcs1.pem",
                        "-out", "rsa-key-pkcs1-aes128.pem"]))
            # dsa-key-pkcs1.pem
            && (!$use_dsa
                || run(app(["openssl", "gendsa", @std_args,
                            "-out", "dsa-key-pkcs1.pem",
                            data_file("dsaparam.pem")])))
            # dsa-key-pkcs1-aes128.pem
            && (!$use_dsa
                || run(app(["openssl", "dsa", @std_args,
                            "-passout", "pass:password", "-aes128",
                            "-in", "dsa-key-pkcs1.pem",
                            "-out", "dsa-key-pkcs1-aes128.pem"])))
            # ec-key-pkcs1.pem (one might think that 'genec' would be practical)
            && (!$use_ecc
                || run(app(["openssl", "ecparam", @std_args,
                            "-genkey",
                            "-name", "prime256v1",
                            "-out", "ec-key-pkcs1.pem"])))
            # ec-key-pkcs1-aes128.pem
            && (!$use_ecc
                || run(app(["openssl", "ec", @std_args,
                            "-passout", "pass:password", "-aes128",
                            "-in", "ec-key-pkcs1.pem",
                            "-out", "ec-key-pkcs1-aes128.pem"])))
            # *-key-pkcs8.pem
            && runall(sub {
                          my $dstfile = shift;
                          (my $srcfile = $dstfile)
                              =~ s/-key-pkcs8\.pem$/-key-pkcs1.pem/i;
                          run(app(["openssl", "pkcs8", @std_args,
                                   "-topk8", "-nocrypt",
                                   "-in", $srcfile, "-out", $dstfile]));
                      }, grep(/-key-pkcs8\.pem$/, @generated_files))
            # *-key-pkcs8-pbes1-sha1-3des.pem
            && runall(sub {
                          my $dstfile = shift;
                          (my $srcfile = $dstfile)
                              =~ s/-key-pkcs8-pbes1-sha1-3des\.pem$
                                  /-key-pkcs8.pem/ix;
                          run(app(["openssl", "pkcs8", @std_args,
                                   "-topk8",
                                   "-passout", "pass:password",
                                   "-v1", "pbeWithSHA1And3-KeyTripleDES-CBC",
                                   "-in", $srcfile, "-out", $dstfile]));
                      }, grep(/-key-pkcs8-pbes1-sha1-3des\.pem$/, @generated_files))
            # *-key-pkcs8-pbes1-md5-des.pem
            && runall(sub {
                          my $dstfile = shift;
                          (my $srcfile = $dstfile)
                              =~ s/-key-pkcs8-pbes1-md5-des\.pem$
                                  /-key-pkcs8.pem/ix;
                          run(app(["openssl", "pkcs8", @std_args,
                                   "-topk8",
                                   "-passout", "pass:password",
                                   "-v1", "pbeWithSHA1And3-KeyTripleDES-CBC",
                                   "-in", $srcfile, "-out", $dstfile]));
                      }, grep(/-key-pkcs8-pbes1-md5-des\.pem$/, @generated_files))
            # *-key-pkcs8-pbes2-sha1.pem
            && runall(sub {
                          my $dstfile = shift;
                          (my $srcfile = $dstfile)
                              =~ s/-key-pkcs8-pbes2-sha1\.pem$
                                  /-key-pkcs8.pem/ix;
                          run(app(["openssl", "pkcs8", @std_args,
                                   "-topk8",
                                   "-passout", "pass:password",
                                   "-v2", "aes256", "-v2prf", "hmacWithSHA1",
                                   "-in", $srcfile, "-out", $dstfile]));
                      }, grep(/-key-pkcs8-pbes2-sha1\.pem$/, @generated_files))
            # *-key-pkcs8-pbes2-sha1.pem
            && runall(sub {
                          my $dstfile = shift;
                          (my $srcfile = $dstfile)
                              =~ s/-key-pkcs8-pbes2-sha256\.pem$
                                  /-key-pkcs8.pem/ix;
                          run(app(["openssl", "pkcs8", @std_args,
                                   "-topk8",
                                   "-passout", "pass:password",
                                   "-v2", "aes256", "-v2prf", "hmacWithSHA256",
                                   "-in", $srcfile, "-out", $dstfile]));
                      }, grep(/-key-pkcs8-pbes2-sha256\.pem$/, @generated_files))
            # *-cert.pem (intermediary for the .p12 inits)
            && run(app(["openssl", "req", "-x509", @std_args,
                        "-config", $cnf, "-noenc",
                        "-key", $cakey, "-out", "cacert.pem"]))
            && runall(sub {
                          my $srckey = shift;
                          (my $dstfile = $srckey) =~ s|-key-pkcs8\.|-cert.|;
                          (my $csr = $dstfile) =~ s|\.pem|.csr|;

                          (run(app(["openssl", "req", "-new", @std_args,
                                    "-config", $cnf, "-section", "userreq",
                                    "-key", $srckey, "-out", $csr]))
                           &&
                           run(app(["openssl", "x509", @std_args,
                                    "-days", "3650",
                                    "-CA", "cacert.pem",
                                    "-CAkey", $cakey,
                                    "-set_serial", time(), "-req",
                                    "-in", $csr, "-out", $dstfile])));
                      }, grep(/-key-pkcs8\.pem$/, @generated_files))
            # *.p12
            && runall(sub {
                          my $dstfile = shift;
                          my ($type, $certpbe_index, $keypbe_index,
                              $macalg_index) =
                              $dstfile =~ m{^(.*)-key-(?|
                                                # cert and key PBE are same
                                                ()             #
                                                ([^-]*-[^-]*)- # key & cert PBE
                                                ([^-]*)        # MACalg
                                            |
                                                # cert and key PBE are not same
                                                ([^-]*-[^-]*)- # cert PBE
                                                ([^-]*-[^-]*)- # key PBE
                                                ([^-]*)        # MACalg
                                            )\.}x;
                          if (!$certpbe_index) {
                              $certpbe_index = $keypbe_index;
                          }
                          my $srckey = "$type-key-pkcs8.pem";
                          my $srccert = "$type-cert.pem";
                          my %pbes =
                              (
                               "sha1-3des" => "pbeWithSHA1And3-KeyTripleDES-CBC",
                               "md5-des" => "pbeWithMD5AndDES-CBC",
                               "aes256-cbc" => "AES-256-CBC",
                              );
                          my %macalgs =
                              (
                               "sha1" => "SHA1",
                               "sha256" => "SHA256",
                              );
                          my $certpbe = $pbes{$certpbe_index};
                          my $keypbe = $pbes{$keypbe_index};
                          my $macalg = $macalgs{$macalg_index};
                          if (!defined($certpbe) || !defined($keypbe)
                              || !defined($macalg)) {
                              print STDERR "Cert PBE for $certpbe_index not defined\n"
                                  unless defined $certpbe;
                              print STDERR "Key PBE for $keypbe_index not defined\n"
                                  unless defined $keypbe;
                              print STDERR "MACALG for $macalg_index not defined\n"
                                  unless defined $macalg;
                              print STDERR "(destination file was $dstfile)\n";
                              return 0;
                          }
                          run(app(["openssl", "pkcs12", @std_args,
                                   "-inkey", $srckey,
                                   "-in", $srccert, "-passout", "pass:password",
                                   "-chain", "-CAfile", "cacert.pem",
                                   "-export", "-macalg", $macalg,
                                   "-certpbe", $certpbe, "-keypbe", $keypbe,
                                   "-out", $dstfile]));
                      }, grep(/\.p12/, @generated_files))
            # *.der (the end all init)
            && runall(sub {
                          my $dstfile = shift;
                          (my $srcfile = $dstfile) =~ s/\.der$/.pem/i;
                          if (! -f $srcfile) {
                              $srcfile = srctop_file("test", $srcfile);
                          }
                          my $infh;
                          unless (open $infh, $srcfile) {
                              return 0;
                          }
                          my $l;
                          while (($l = <$infh>) !~ /^-----BEGIN\s/
                                 || $l =~ /^-----BEGIN.*PARAMETERS-----/) {
                          }
                          my $b64 = "";
                          while (($l = <$infh>) !~ /^-----END\s/) {
                              $l =~ s|\R$||;
                              $b64 .= $l unless $l =~ /:/;
                          }
                          close $infh;
                          my $der = decode_base64($b64);
                          unless (length($b64) / 4 * 3 - length($der) < 3) {
                              print STDERR "Length error, ",length($b64),
                                  " bytes of base64 became ",length($der),
                                  " bytes of der? ($srcfile => $dstfile)\n";
                              return 0;
                          }
                          my $outfh;
                          unless (open $outfh, ">:raw", $dstfile) {
                              return 0;
                          }
                          print $outfh $der;
                          close $outfh;
                          return 1;
                      }, grep(/\.der$/, @generated_files))
            && runall(sub {
                          my $srcfile = shift;
                          my $dstfile = $generated_file_files{$srcfile};

                          unless (copy srctop_file($srcfile), $dstfile) {
                              warn "$!\n";
                              return 0;
                          }
                          return 1;
                      }, keys %generated_file_files)
           );
}

sub init_rehash {
    return (
            mkdir(catdir(curdir(), 'rehash'))
            && copy(srctop_file('test', 'testx509.pem'),
                    catdir(curdir(), 'rehash'))
            && copy(srctop_file('test', 'testcrl.pem'),
                    catdir(curdir(), 'rehash'))
            && run(app(['openssl', 'rehash', catdir(curdir(), 'rehash')]))
           );
}

sub runall {
    my ($function, @items) = @_;

    foreach (@items) {
        return 0 unless $function->($_);
    }
    return 1;
}

# According to RFC8089, a relative file: path is invalid.  We still produce
# them for testing purposes.
sub to_file_uri {
    my ($file, $isdir, $authority) = @_;
    my $vol;
    my $dir;

    die "to_file_uri: No file given\n" if !defined($file) || $file eq '';

    ($vol, $dir, $file) = File::Spec->splitpath($file, $isdir // 0);

    # Make sure we have a Unix style directory.
    $dir = join('/', File::Spec->splitdir($dir));
    # Canonicalise it (note: it seems to be only needed on Unix)
    while (1) {
        my $newdir = $dir;
        $newdir =~ s|/[^/]*[^/\.]+[^/]*/\.\./|/|g;
        last if $newdir eq $dir;
        $dir = $newdir;
    }
    # Take care of the corner cases the loop can't handle, and that $dir
    # ends with a / unless it's empty
    $dir =~ s|/[^/]*[^/\.]+[^/]*/\.\.$|/|;
    $dir =~ s|^[^/]*[^/\.]+[^/]*/\.\./|/|;
    $dir =~ s|^[^/]*[^/\.]+[^/]*/\.\.$||;
    if ($isdir // 0) {
        $dir =~ s|/$|| if $dir ne '/';
    } else {
        $dir .= '/' if $dir ne '' && $dir !~ m|/$|;
    }

    # If the file system has separate volumes (at present, Windows and VMS)
    # we need to handle them.  In URIs, they are invariably the first
    # component of the path, which is always absolute.
    # On VMS, user:[foo.bar] translates to /user/foo/bar
    # On Windows, c:\Users\Foo translates to /c:/Users/Foo
    if ($vol ne '') {
        $vol =~ s|:||g if ($^O eq "VMS");
        $dir = '/' . $dir if $dir ne '' && $dir !~ m|^/|;
        $dir = '/' . $vol . $dir;
    }
    $file = $dir . $file;

    return "file://$authority$file" if defined $authority;
    return "file:$file";
}

sub to_abs_file {
    my ($file) = @_;

    return File::Spec->rel2abs($file);
}

sub to_abs_file_uri {
    my ($file, $isdir, $authority) = @_;

    die "to_abs_file_uri: No file given\n" if !defined($file) || $file eq '';
    return to_file_uri(to_abs_file($file), $isdir, $authority);
}
