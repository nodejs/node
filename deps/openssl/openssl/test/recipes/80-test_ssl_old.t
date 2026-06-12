#! /usr/bin/env perl
# Copyright 2015-2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use POSIX;
use File::Basename;
use File::Copy;
use OpenSSL::Test qw/:DEFAULT with bldtop_file bldtop_dir srctop_file srctop_dir cmdstr data_file result_dir result_file/;
use OpenSSL::Test::Utils;

BEGIN {
setup("test_ssl_old");
}

use lib srctop_dir('Configurations');
use lib bldtop_dir('.');

my $no_fips = disabled('fips') || ($ENV{NO_FIPS} // 0);
my ($no_rsa, $no_dsa, $no_dh, $no_ec, $no_psk,
    $no_ssl3, $no_tls1, $no_tls1_1, $no_tls1_2, $no_tls1_3,
    $no_dtls, $no_dtls1, $no_dtls1_2, $no_ct) =
    anydisabled qw/rsa dsa dh ec psk
                   ssl3 tls1 tls1_1 tls1_2 tls1_3
                   dtls dtls1 dtls1_2 ct/;
#If ec and dh are disabled then don't use TLSv1.3
$no_tls1_3 = 1 if (!$no_tls1_3 && $no_ec && $no_dh);
my $no_anytls = alldisabled(available_protocols("tls"));
my $no_anydtls = alldisabled(available_protocols("dtls"));

plan skip_all => "No SSL/TLS/DTLS protocol is support by this OpenSSL build"
    if $no_anytls && $no_anydtls;

my $dsaallow = '1';
my $digest = "-sha1";
my @reqcmd = ("openssl", "req");
my @x509cmd = ("openssl", "x509", $digest);
my @verifycmd = ("openssl", "verify");
my @genpkeycmd = ("openssl", "genpkey");
my $dummycnf = srctop_file("apps", "openssl.cnf");

my $cnf = srctop_file("test", "ca-and-certs.cnf");
my $CAkey = srctop_file("test", "certs", "ca-key.pem"); # "keyCA.ss"
my $CAcert="certCA.ss";
my $CAserial="certCA.srl";
my $CAreq="reqCA.ss";
my $CAreq2="req2CA.ss"; # temp
my $Ukey = srctop_file("test", "certs", "ee-key.pem"); # "keyU.ss";
my $Ureq="reqU.ss";
my $Ucert="certU.ss";
my $Dkey="keyD.ss";
my $Dreq="reqD.ss";
my $Dcert="certD.ss";
my $Ekey="keyE.ss";
my $Ereq="reqE.ss";
my $Ecert="certE.ss";

my $proxycnf=srctop_file("test", "proxy.cnf");
my $P1key= srctop_file("test", "certs", "alt1-key.pem"); # "keyP1.ss";
my $P1req="reqP1.ss";
my $P1cert="certP1.ss";
my $P1intermediate="tmp_intP1.ss";
my $P2key= srctop_file("test", "certs", "alt2-key.pem"); # "keyP2.ss";
my $P2req="reqP2.ss";
my $P2cert="certP2.ss";
my $P2intermediate="tmp_intP2.ss";

my $server_sess="server.ss";
my $client_sess="client.ss";

# ssl_old_test.c is deprecated in favour of the new framework in ssl_test.c
# If you're adding tests here, you probably want to convert them to the
# new format in ssl_test.c and add recipes to 80-test_ssl_new.t instead.
plan tests =>
   ($no_fips ? 0 : 7)     # testssl with fips provider
    + 1                   # For testss
    + 5                   # For the testssl with default provider
    + 1                   # For security level 0 failure tests
    ;

subtest 'test_ss' => sub {
    if (testss()) {
        open OUT, ">", "intP1.ss";
        copy($CAcert, \*OUT); copy($Ucert, \*OUT);
        close OUT;

        open OUT, ">", "intP2.ss";
        copy($CAcert, \*OUT); copy($Ucert, \*OUT); copy($P1cert, \*OUT);
        close OUT;
    }
};

note('test_ssl_old -- key U');
my $configfile = srctop_file("test","default-and-legacy.cnf");
if (disabled("legacy")) {
    $configfile = srctop_file("test","default.cnf");
}

testssl($Ukey, $Ucert, $CAcert, "default", $configfile);
unless ($no_fips) {
    # Read in a text $infile and replace the regular expression in $srch with the
    # value in $repl and output to a new file $outfile.
    sub replace_line_file_internal {

        my ($infile, $srch, $repl, $outfile) = @_;
        my $msg;

        open(my $in, "<", $infile) or return 0;
        read($in, $msg, 1024);
        close $in;

        $msg =~ s/$srch/$repl/;

        open(my $fh, ">", $outfile) or return 0;
        print $fh $msg;
        close $fh;
        return 1;
    }

    # Read in the text input file $infile
    # and replace a single Key = Value line with a new value in $value.
    # OR remove the Key = Value line if the passed in $value is empty.
    # and then output a new file $outfile.
    # $key is the Key to find
    sub replace_kv_file {
        my ($infile, $key, $value, $outfile) = @_;
        my $srch = qr/$key\s*=\s*\S*\n/;
        my $rep;
        if ($value eq "") {
            $rep = "";
        } else {
           $rep = "$key = $value\n";
        }
        return replace_line_file_internal($infile, $srch, $rep, $outfile);
    }

    # Read in the text $input file
    # and search for the $key and replace with $newkey
    # and then output a new file $outfile.
    sub replace_line_file {
        my ($infile, $key, $newkey, $outfile) = @_;
        my $srch = qr/$key/;
        my $rep = "$newkey";
        return replace_line_file_internal($infile,
                                          $srch, $rep, $outfile);
    }

    # Rewrite the module configuration to all PKCS#1 v1.5 padding
    my $fipsmodcfg_filename = "fipsmodule.cnf";
    my $fipsmodcfg = bldtop_file("test", $fipsmodcfg_filename);
    my $provconf = srctop_file("test", "fips-and-base.cnf");
    my $provconfnew = result_file("fips-and-base-temp.cnf");
    my $fipsmodcfgnew_filename = "fipsmodule_mod.cnf";
    my $fipsmodcfgnew = result_file($fipsmodcfgnew_filename);
    $ENV{OPENSSL_CONF_INCLUDE} = result_dir();
    ok(replace_kv_file($fipsmodcfg,
                       'rsa-pkcs15-pad-disabled', '0',
                       $fipsmodcfgnew)
       && replace_line_file($provconf,
                            $fipsmodcfg_filename, $fipsmodcfgnew_filename,
                            $provconfnew));

    testssl($Ukey, $Ucert, $CAcert, "fips", $provconfnew);
}

# -----------
# subtest functions
sub testss {
    my @req_dsa = ("-newkey",
                   "dsa:".data_file("dsa2048.pem"));
    my $dsaparams = data_file("dsa2048.pem");
    my @req_new;
    if ($no_rsa) {
        @req_new = @req_dsa;
    } else {
        @req_new = ("-new");
    }

    plan tests => 17;

  SKIP: {
      skip 'failure', 16 unless
          ok(run(app([@reqcmd, "-config", $cnf,
                      "-out", $CAreq, "-key", $CAkey,
                      @req_new])),
             'make cert request');

      skip 'failure', 15 unless
          ok(run(app([@x509cmd, "-CAcreateserial", "-in", $CAreq, "-days", "30",
                      "-req", "-out", $CAcert, "-signkey", $CAkey,
                      "-extfile", $cnf, "-extensions", "v3_ca"],
                     stdout => "err.ss")),
             'convert request into self-signed cert');

      skip 'failure', 14 unless
          ok(run(app([@x509cmd, "-in", $CAcert,
                      "-x509toreq", "-signkey", $CAkey, "-out", $CAreq2],
                     stdout => "err.ss")),
             'convert cert into a cert request');

      skip 'failure', 13 unless
          ok(run(app([@reqcmd, "-config", $dummycnf,
                      "-verify", "-in", $CAreq, "-noout"])),
             'verify request 1');


      skip 'failure', 12 unless
          ok(run(app([@reqcmd, "-config", $dummycnf,
                      "-verify", "-in", $CAreq2, "-noout"])),
             'verify request 2');

      skip 'failure', 11 unless
          ok(run(app([@verifycmd, "-CAfile", $CAcert, $CAcert])),
             'verify signature');

      skip 'failure', 10 unless
          ok(run(app([@reqcmd, "-config", $cnf, "-section", "userreq",
                      "-out", $Ureq, "-key", $Ukey, @req_new],
                     stdout => "err.ss")),
             'make a user cert request');

      skip 'failure', 9 unless
          ok(run(app([@x509cmd, "-CAcreateserial", "-in", $Ureq, "-days", "30",
                      "-req", "-out", $Ucert,
                      "-CA", $CAcert, "-CAkey", $CAkey, "-CAserial", $CAserial,
                      "-extfile", $cnf, "-extensions", "v3_ee"],
                     stdout => "err.ss"))
             && run(app([@verifycmd, "-CAfile", $CAcert, $Ucert])),
             'sign user cert request');

      skip 'failure', 8 unless
          ok(run(app([@x509cmd,
                      "-subject", "-issuer", "-startdate", "-enddate",
                      "-noout", "-in", $Ucert])),
             'Certificate details');

      skip 'failure', 7 unless
          subtest 'DSA certificate creation' => sub {
              plan skip_all => "skipping DSA certificate creation"
                  if $no_dsa;

              plan tests => 5;

            SKIP: {
                $ENV{CN2} = "DSA Certificate";
                skip 'failure', 4 unless
                    ok(run(app([@genpkeycmd, "-out", $Dkey,
                                "-paramfile", $dsaparams],
                               stdout => "err.ss")),
                       "make a DSA key");
                skip 'failure', 3 unless
                    ok(run(app([@reqcmd, "-new", "-config", $cnf,
                                "-section", "userreq",
                                "-out", $Dreq, "-key", $Dkey],
                               stdout => "err.ss")),
                       "make a DSA user cert request");
                skip 'failure', 2 unless
                    ok(run(app([@x509cmd, "-CAcreateserial",
                                "-in", $Dreq,
                                "-days", "30",
                                "-req",
                                "-out", $Dcert,
                                "-CA", $CAcert, "-CAkey", $CAkey,
                                "-CAserial", $CAserial,
                                "-extfile", $cnf,
                                "-extensions", "v3_ee_dsa"],
                               stdout => "err.ss")),
                       "sign DSA user cert request");
                skip 'failure', 1 unless
                    ok(run(app([@verifycmd, "-CAfile", $CAcert, $Dcert])),
                       "verify DSA user cert");
                skip 'failure', 0 unless
                    ok(run(app([@x509cmd,
                                "-subject", "-issuer",
                                "-startdate", "-enddate", "-noout",
                                "-in", $Dcert])),
                       "DSA Certificate details");
              }
      };

      skip 'failure', 6 unless
          subtest 'ECDSA/ECDH certificate creation' => sub {
              plan skip_all => "skipping ECDSA/ECDH certificate creation"
                  if $no_ec;

              plan tests => 5;

            SKIP: {
                $ENV{CN2} = "ECDSA Certificate";
                skip 'failure', 4 unless
                    ok(run(app(["openssl", "genpkey", "-genparam",
                                "-algorithm", "EC",
                                "-pkeyopt", "ec_paramgen_curve:P-256",
                                "-pkeyopt", "ec_param_enc:named_curve",
                                "-out", "ecp.ss"])),
                       "make EC parameters");
                skip 'failure', 3 unless
                    ok(run(app([@reqcmd, "-config", $cnf,
                                "-section", "userreq",
                                "-out", $Ereq, "-keyout", $Ekey,
                                "-newkey", "ec:ecp.ss"],
                               stdout => "err.ss")),
                       "make a ECDSA/ECDH user cert request");
                skip 'failure', 2 unless
                    ok(run(app([@x509cmd, "-CAcreateserial",
                                "-in", $Ereq,
                                "-days", "30",
                                "-req",
                                "-out", $Ecert,
                                "-CA", $CAcert, "-CAkey", $CAkey,
                                "-CAserial", $CAserial,
                                "-extfile", $cnf,
                                "-extensions", "v3_ee_ec"],
                               stdout => "err.ss")),
                       "sign ECDSA/ECDH user cert request");
                skip 'failure', 1 unless
                    ok(run(app([@verifycmd, "-CAfile", $CAcert, $Ecert])),
                       "verify ECDSA/ECDH user cert");
                skip 'failure', 0 unless
                    ok(run(app([@x509cmd,
                                "-subject", "-issuer",
                                "-startdate", "-enddate", "-noout",
                                "-in", $Ecert])),
                       "ECDSA Certificate details");
              }
      };

      skip 'failure', 5 unless
          ok(run(app([@reqcmd, "-config", $proxycnf,
                      "-out", $P1req, "-key", $P1key, @req_new],
                     stdout => "err.ss")),
             'make a proxy cert request');


      skip 'failure', 4 unless
          ok(run(app([@x509cmd, "-CAcreateserial", "-in", $P1req, "-days", "30",
                      "-req", "-out", $P1cert,
                      "-CA", $Ucert, "-CAkey", $Ukey,
                      "-extfile", $proxycnf, "-extensions", "proxy"],
                     stdout => "err.ss")),
             'sign proxy with user cert');

      copy($Ucert, $P1intermediate);
      run(app([@verifycmd, "-CAfile", $CAcert,
               "-untrusted", $P1intermediate, $P1cert]));
      ok(run(app([@x509cmd,
                  "-subject", "-issuer", "-startdate", "-enddate",
                  "-noout", "-in", $P1cert])),
         'Certificate details');

      skip 'failure', 2 unless
          ok(run(app([@reqcmd, "-config", $proxycnf, "-section", "proxy2_req",
                      "-out", $P2req, "-key", $P2key,
                      @req_new],
                     stdout => "err.ss")),
             'make another proxy cert request');


      skip 'failure', 1 unless
          ok(run(app([@x509cmd, "-CAcreateserial", "-in", $P2req, "-days", "30",
                      "-req", "-out", $P2cert,
                      "-CA", $P1cert, "-CAkey", $P1key,
                      "-extfile", $proxycnf, "-extensions", "proxy_2"],
                     stdout => "err.ss")),
             'sign second proxy cert request with the first proxy cert');


      open OUT, ">", $P2intermediate;
      copy($Ucert, \*OUT); copy($P1cert, \*OUT);
      close OUT;
      run(app([@verifycmd, "-CAfile", $CAcert,
               "-untrusted", $P2intermediate, $P2cert]));
      ok(run(app([@x509cmd,
                  "-subject", "-issuer", "-startdate", "-enddate",
                  "-noout", "-in", $P2cert])),
         'Certificate details');
    }
}

sub testssl {
    my ($key, $cert, $CAtmp, $provider, $configfile) = @_;
    my @CA = $CAtmp ? ("-CAfile", $CAtmp) : ("-CApath", bldtop_dir("certs"));
    my @providerflags = ("-provider", $provider);

    if ($provider eq "default" && !disabled("legacy")) {
        push @providerflags, "-provider", "legacy";
    }

    $dsaallow = '1';
    if  ($provider eq "fips") {
        run(test(["fips_version_test", "-config", $configfile, "<3.4.0"]),
              capture => 1, statusvar => \$dsaallow);
    }

    my @ssltest = ("ssl_old_test",
                   "-s_key", $key, "-s_cert", $cert,
                   "-c_key", $key, "-c_cert", $cert,
                   "-config", $configfile,
                   @providerflags);


    my $serverinfo = srctop_file("test","serverinfo.pem");

    my $dsa_cert = 0;
    if (grep /DSA Public Key/, run(app(["openssl", "x509", "-in", $cert,
                                        "-text", "-noout"]), capture => 1)) {
        $dsa_cert = 1;
    }

    subtest 'standard SSL tests' => sub {
        ######################################################################
        plan tests => 19;

      SKIP: {
          skip "SSLv3 is not supported by this OpenSSL build", 4
              if disabled("ssl3");

          skip "SSLv3 is not supported by the FIPS provider", 4
              if $provider eq "fips";

          ok(run(test([@ssltest, "-bio_pair", "-ssl3"])),
             'test sslv3 via BIO pair');
          ok(run(test([@ssltest, "-bio_pair", "-ssl3", "-server_auth", @CA])),
             'test sslv3 with server authentication via BIO pair');
          ok(run(test([@ssltest, "-bio_pair", "-ssl3", "-client_auth", @CA])),
             'test sslv3 with client authentication via BIO pair');
          ok(run(test([@ssltest, "-bio_pair", "-ssl3", "-server_auth", "-client_auth", @CA])),
             'test sslv3 with both server and client authentication via BIO pair');
        }

      SKIP: {
          skip "Neither SSLv3 nor any TLS version are supported by this OpenSSL build", 1
              if $no_anytls;

          ok(run(test([@ssltest, "-bio_pair"])),
             'test sslv2/sslv3 via BIO pair');
        }

      SKIP: {
          skip "Neither SSLv3 nor any TLS version are supported by this OpenSSL build", 14
              if $no_anytls;

        SKIP: {
            skip "skipping test of sslv2/sslv3 w/o (EC)DHE test", 1 if $dsa_cert;

            ok(run(test([@ssltest, "-bio_pair", "-no_dhe", "-no_ecdhe"])),
               'test sslv2/sslv3 w/o (EC)DHE via BIO pair');
          }

        SKIP: {
            skip "skipping dhe1024dsa test", 1
                if ($no_dh);

            ok(run(test([@ssltest, "-bio_pair", "-dhe1024dsa", "-v"])),
               'test sslv2/sslv3 with 1024bit DHE via BIO pair');
          }

          ok(run(test([@ssltest, "-bio_pair", "-server_auth", @CA])),
             'test sslv2/sslv3 with server authentication');
          ok(run(test([@ssltest, "-bio_pair", "-client_auth", @CA])),
             'test sslv2/sslv3 with client authentication via BIO pair');
          ok(run(test([@ssltest, "-bio_pair", "-server_auth", "-client_auth", @CA])),
             'test sslv2/sslv3 with both client and server authentication via BIO pair');
          ok(run(test([@ssltest, "-bio_pair", "-server_auth", "-client_auth", "-app_verify", @CA])),
             'test sslv2/sslv3 with both client and server authentication via BIO pair and app verify');

        SKIP: {
            skip "No IPv4 available on this machine", 4
                unless !disabled("sock") && have_IPv4();
            ok(run(test([@ssltest, "-ipv4"])),
               'test TLS via IPv4');
            ok(run(test([@ssltest, "-ipv4", "-client_ktls"])),
               'test TLS via IPv4 + ktls(client)');
            ok(run(test([@ssltest, "-ipv4", "-server_ktls"])),
               'test TLS via IPv4 + ktls(server)');
            ok(run(test([@ssltest, "-ipv4", "-client_ktls", "-server_ktls"])),
               'test TLS via IPv4 + ktls');
          }

        SKIP: {
            skip "No IPv6 available on this machine", 4
                unless !disabled("sock") && have_IPv6();
            ok(run(test([@ssltest, "-ipv6"])),
               'test TLS via IPv6');
            ok(run(test([@ssltest, "-ipv6", "-client_ktls"])),
               'test TLS via IPv6 + ktls(client)');
            ok(run(test([@ssltest, "-ipv6", "-server_ktls"])),
               'test TLS via IPv6 + ktls(client)');
            ok(run(test([@ssltest, "-ipv6", "-client_ktls", "-server_ktls"])),
               'test TLS via IPv6 + ktls');
          }
        }
    };

    subtest "Testing ciphersuites" => sub {

        my @exkeys = ();
        my $ciphers = '-PSK:-SRP:@SECLEVEL=0';

        if (!$no_dsa && $dsaallow == '1') {
            push @exkeys, "-s_cert", "certD.ss", "-s_key", $Dkey;
        }

        if (!$no_ec) {
            push @exkeys, "-s_cert", "certE.ss", "-s_key", $Ekey;
        }

        my @protocols = ();
        # We only use the flags that ssl_old_test understands
        push @protocols, "-tls1_3" unless $no_tls1_3;
        push @protocols, "-tls1_2" unless $no_tls1_2;
        push @protocols, "-tls1" unless $no_tls1 || $provider eq "fips";
        push @protocols, "-ssl3" unless $no_ssl3 || $provider eq "fips";
        my $protocolciphersuitecount = 0;
        my %ciphersuites = ();
        my %ciphersstatus = ();
        #There's no "-config" option to the ciphers command so we set the
        #environment variable instead
        my $opensslconf = $ENV{OPENSSL_CONF};
        $ENV{OPENSSL_CONF} = $configfile;
        foreach my $protocol (@protocols) {
            my $ciphersstatus = undef;
            my @ciphers = run(app(["openssl", "ciphers", "-s", $protocol,
                                   @providerflags,
                                   "ALL:$ciphers"]),
                                   capture => 1, statusvar => \$ciphersstatus);
            $ciphersstatus{$protocol} = $ciphersstatus;
            if ($ciphersstatus) {
                $ciphersuites{$protocol} = [ map { s|\R||; split(/:/, $_) }
                                    @ciphers ];
                $protocolciphersuitecount += scalar @{$ciphersuites{$protocol}};
            }
        }
        $ENV{OPENSSL_CONF} = $opensslconf;

        plan skip_all => "None of the ciphersuites to test are available in this OpenSSL build"
            if $protocolciphersuitecount + scalar(keys %ciphersuites) == 0;

        # The count of protocols is because in addition to the ciphersuites
        # we got above, we're running a weak DH test for each protocol (except
        # TLSv1.3)
        my $testcount = scalar(@protocols) + $protocolciphersuitecount
                        + scalar(keys %ciphersuites);
        $testcount-- unless $no_tls1_3;
        plan tests => $testcount;

        foreach my $protocol (@protocols) {
            ok($ciphersstatus{$protocol}, "Getting ciphers for $protocol");
        }

        foreach my $protocol (sort keys %ciphersuites) {
            note "Testing ciphersuites for $protocol";
            # ssl_old_test doesn't know -tls1_3, but that's fine, since that's
            # the default choice if TLSv1.3 enabled
            my $flag = $protocol eq "-tls1_3" ? "" : $protocol;
            my $ciphersuites = "";
            foreach my $cipher (@{$ciphersuites{$protocol}}) {
                if ($dsaallow == '0' && index($cipher, "DSS") != -1) {
                    # DSA is not allowed in FIPS 140-3
                    note "*****SKIPPING $protocol $cipher";
                    ok(1);
                } elsif ($protocol eq "-ssl3" && $cipher =~ /ECDH/ ) {
                    note "*****SKIPPING $protocol $cipher";
                    ok(1);
                } else {
                    if ($protocol eq "-tls1_3") {
                        $ciphersuites = $cipher;
                        $cipher = "";
                    } else {
                        $cipher = $cipher.':@SECLEVEL=0';
                    }
                    ok(run(test([@ssltest, @exkeys, "-cipher",
                                 $cipher,
                                 "-ciphersuites", $ciphersuites,
                                 $flag || ()])),
                       "Testing $cipher");
                }
            }
            next if $protocol eq "-tls1_3";

          SKIP: {
              skip "skipping dhe512 test", 1
                  if ($no_dh);

              is(run(test([@ssltest,
                           "-s_cipher", "EDH",
                           "-c_cipher", 'EDH:@SECLEVEL=1',
                           "-dhe512",
                           $protocol])), 0,
                 "testing connection with weak DH, expecting failure");
            }
        }
    };

    subtest 'SSL security level failure tests' => sub {
        ######################################################################
        plan tests => 3;

      SKIP: {
          skip "SSLv3 is not supported by this OpenSSL build", 1
              if disabled("ssl3");

          skip "SSLv3 is not supported by the FIPS provider", 1
              if $provider eq "fips";

          is(run(test([@ssltest, "-bio_pair", "-ssl3", "-cipher", '@SECLEVEL=1'])),
             0, "test sslv3 fails at security level 1, expecting failure");
        }

      SKIP: {
          skip "TLSv1.0 is not supported by this OpenSSL build", 1
              if $no_tls1;

          skip "TLSv1.0 is not supported by the FIPS provider", 1
              if $provider eq "fips";

          is(run(test([@ssltest, "-bio_pair", "-tls1", "-cipher", '@SECLEVEL=1'])),
             0, 'test tls1 fails at security level 1, expecting failure');
        }

      SKIP: {
          skip "TLSv1.1 is not supported by this OpenSSL build", 1
              if $no_tls1_1;

          skip "TLSv1.1 is not supported by the FIPS provider", 1
              if $provider eq "fips";

          is(run(test([@ssltest, "-bio_pair", "-tls1_1", "-cipher", '@SECLEVEL=1'])),
             0, 'test tls1.1 fails at security level 1, expecting failure');
        }
    };

    subtest 'RSA/(EC)DHE/PSK tests' => sub {
        ######################################################################

        plan tests => 10;

      SKIP: {
            skip "TLSv1.0 is not supported by this OpenSSL build", 6
                if $no_tls1 || $provider eq "fips";

        SKIP: {
            skip "skipping anonymous DH tests", 1
                if ($no_dh);

            ok(run(test([@ssltest, "-v", "-bio_pair", "-tls1", "-cipher", "ADH", "-dhe1024dsa", "-num", "10", "-f", "-time"])),
               'test tlsv1 with 1024bit anonymous DH, multiple handshakes');
          }

        SKIP: {
            skip "skipping RSA tests", 2
                if $no_rsa;

            ok(run(test(["ssl_old_test", "-provider", "default", "-v", "-bio_pair", "-tls1", "-s_cert", srctop_file("apps","server2.pem"), "-no_dhe", "-no_ecdhe", "-num", "10", "-f", "-time"])),
               'test tlsv1 with 1024bit RSA, no (EC)DHE, multiple handshakes');

            skip "skipping RSA+DHE tests", 1
                if $no_dh;

            ok(run(test(["ssl_old_test", "-provider", "default", "-v", "-bio_pair", "-tls1", "-s_cert", srctop_file("apps","server2.pem"), "-dhe1024dsa", "-num", "10", "-f", "-time"])),
               'test tlsv1 with 1024bit RSA, 1024bit DHE, multiple handshakes');
          }

        SKIP: {
            skip "skipping PSK tests", 2
                if ($no_psk);

            ok(run(test([@ssltest, "-tls1", "-cipher", "PSK", "-psk", "abc123"])),
               'test tls1 with PSK');

            ok(run(test([@ssltest, "-bio_pair", "-tls1", "-cipher", "PSK", "-psk", "abc123"])),
               'test tls1 with PSK via BIO pair');
          }

        SKIP: {
            skip "skipping auto DH PSK tests", 1
                if ($no_dh || $no_psk);

            ok(run(test(['ssl_old_test', '-psk', '0102030405', '-cipher', '@SECLEVEL=2:DHE-PSK-AES128-CCM'])),
               'test auto DH meets security strength');
          }
	}

      SKIP: {
            skip "TLSv1.2 is not supported by this OpenSSL build", 4
                if $no_tls1_2;

        SKIP: {
            skip "skipping auto DHE PSK test at SECLEVEL 3", 1
                if ($no_dh || $no_psk);

            ok(run(test(['ssl_old_test', '-tls1_2', '-dhe4096', '-psk', '0102030405', '-cipher', '@SECLEVEL=3:DHE-PSK-AES256-CBC-SHA384'])),
               'test auto DHE PSK meets security strength');
          }

        SKIP: {
            skip "skipping auto ECDHE PSK test at SECLEVEL 3", 1
                if ($no_ec || $no_psk);

            ok(run(test(['ssl_old_test', '-tls1_2', '-no_dhe', '-psk', '0102030405', '-cipher', '@SECLEVEL=3:ECDHE-PSK-AES256-CBC-SHA384'])),
               'test auto ECDHE PSK meets security strength');
          }

        SKIP: {
            skip "skipping no RSA PSK at SECLEVEL 3 test", 1
                if ($no_rsa || $no_psk);

            ok(!run(test(['ssl_old_test', '-tls1_2', '-no_dhe', '-psk', '0102030405', '-cipher', '@SECLEVEL=3:RSA-PSK-AES256-CBC-SHA384'])),
               'test auto RSA PSK does not meet security level 3 requirements (PFS)');
          }

        SKIP: {
            skip "skipping no PSK at SECLEVEL 3 test", 1
                if ($no_psk);

            ok(!run(test(['ssl_old_test', '-tls1_2', '-no_dhe', '-psk', '0102030405', '-cipher', '@SECLEVEL=3:PSK-AES256-CBC-SHA384'])),
               'test auto PSK does not meet security level 3 requirements (PFS)');
          }
	}

    };

    subtest 'Custom Extension tests' => sub {
        ######################################################################

        plan tests => 1;

      SKIP: {
          skip "TLSv1.0 is not supported by this OpenSSL build", 1
              if $no_tls1 || $provider eq "fips";

          ok(run(test([@ssltest, "-bio_pair", "-tls1", "-custom_ext"])),
             'test tls1 with custom extensions');
        }
    };

    subtest 'Serverinfo tests' => sub {
        ######################################################################

        plan tests => 5;

      SKIP: {
          skip "TLSv1.0 is not supported by this OpenSSL build", 5
              if $no_tls1 || $provider eq "fips";

          note('echo test tls1 with serverinfo');
          ok(run(test([@ssltest, "-bio_pair", "-tls1", "-serverinfo_file", $serverinfo])));
          ok(run(test([@ssltest, "-bio_pair", "-tls1", "-serverinfo_file", $serverinfo, "-serverinfo_sct"])));
          ok(run(test([@ssltest, "-bio_pair", "-tls1", "-serverinfo_file", $serverinfo, "-serverinfo_tack"])));
          ok(run(test([@ssltest, "-bio_pair", "-tls1", "-serverinfo_file", $serverinfo, "-serverinfo_sct", "-serverinfo_tack"])));
          ok(run(test([@ssltest, "-bio_pair", "-tls1", "-custom_ext", "-serverinfo_file", $serverinfo, "-serverinfo_sct", "-serverinfo_tack"])));
        }
    };
}
