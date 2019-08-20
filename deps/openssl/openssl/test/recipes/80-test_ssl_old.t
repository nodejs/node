#! /usr/bin/env perl
# Copyright 2015-2018 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use POSIX;
use File::Basename;
use File::Copy;
use OpenSSL::Test qw/:DEFAULT with bldtop_file srctop_file cmdstr/;
use OpenSSL::Test::Utils;

setup("test_ssl");

$ENV{CTLOG_FILE} = srctop_file("test", "ct", "log_list.conf");

my ($no_rsa, $no_dsa, $no_dh, $no_ec, $no_psk,
    $no_ssl3, $no_tls1, $no_tls1_1, $no_tls1_2, $no_tls1_3,
    $no_dtls, $no_dtls1, $no_dtls1_2, $no_ct) =
    anydisabled qw/rsa dsa dh ec psk
                   ssl3 tls1 tls1_1 tls1_2 tls1_3
                   dtls dtls1 dtls1_2 ct/;
my $no_anytls = alldisabled(available_protocols("tls"));
my $no_anydtls = alldisabled(available_protocols("dtls"));

plan skip_all => "No SSL/TLS/DTLS protocol is support by this OpenSSL build"
    if $no_anytls && $no_anydtls;

my $digest = "-sha1";
my @reqcmd = ("openssl", "req");
my @x509cmd = ("openssl", "x509", $digest);
my @verifycmd = ("openssl", "verify");
my @gendsacmd = ("openssl", "gendsa");
my $dummycnf = srctop_file("apps", "openssl.cnf");

my $CAkey = "keyCA.ss";
my $CAcert="certCA.ss";
my $CAserial="certCA.srl";
my $CAreq="reqCA.ss";
my $CAconf=srctop_file("test","CAss.cnf");
my $CAreq2="req2CA.ss";	# temp

my $Uconf=srctop_file("test","Uss.cnf");
my $Ukey="keyU.ss";
my $Ureq="reqU.ss";
my $Ucert="certU.ss";

my $Dkey="keyD.ss";
my $Dreq="reqD.ss";
my $Dcert="certD.ss";

my $Ekey="keyE.ss";
my $Ereq="reqE.ss";
my $Ecert="certE.ss";

my $P1conf=srctop_file("test","P1ss.cnf");
my $P1key="keyP1.ss";
my $P1req="reqP1.ss";
my $P1cert="certP1.ss";
my $P1intermediate="tmp_intP1.ss";

my $P2conf=srctop_file("test","P2ss.cnf");
my $P2key="keyP2.ss";
my $P2req="reqP2.ss";
my $P2cert="certP2.ss";
my $P2intermediate="tmp_intP2.ss";

my $server_sess="server.ss";
my $client_sess="client.ss";

# ssltest_old.c is deprecated in favour of the new framework in ssl_test.c
# If you're adding tests here, you probably want to convert them to the
# new format in ssl_test.c and add recipes to 80-test_ssl_new.t instead.
plan tests =>
    1				# For testss
    +5  			# For the first testssl
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

note('test_ssl -- key U');
testssl("keyU.ss", $Ucert, $CAcert);

# -----------
# subtest functions
sub testss {
    open RND, ">>", ".rnd";
    print RND "string to make the random number generator think it has randomness";
    close RND;

    my @req_dsa = ("-newkey",
                   "dsa:".srctop_file("apps", "dsa1024.pem"));
    my $dsaparams = srctop_file("apps", "dsa1024.pem");
    my @req_new;
    if ($no_rsa) {
	@req_new = @req_dsa;
    } else {
	@req_new = ("-new");
    }

    plan tests => 17;

  SKIP: {
      skip 'failure', 16 unless
	  ok(run(app([@reqcmd, "-config", $CAconf,
		      "-out", $CAreq, "-keyout", $CAkey,
		      @req_new])),
	     'make cert request');

      skip 'failure', 15 unless
	  ok(run(app([@x509cmd, "-CAcreateserial", "-in", $CAreq, "-days", "30",
		      "-req", "-out", $CAcert, "-signkey", $CAkey,
		      "-extfile", $CAconf, "-extensions", "v3_ca"],
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
	  ok(run(app([@reqcmd, "-config", $Uconf,
		      "-out", $Ureq, "-keyout", $Ukey, @req_new],
		     stdout => "err.ss")),
	     'make a user cert request');

      skip 'failure', 9 unless
	  ok(run(app([@x509cmd, "-CAcreateserial", "-in", $Ureq, "-days", "30",
		      "-req", "-out", $Ucert,
		      "-CA", $CAcert, "-CAkey", $CAkey, "-CAserial", $CAserial,
		      "-extfile", $Uconf, "-extensions", "v3_ee"],
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
                    ok(run(app([@gendsacmd, "-out", $Dkey,
                                $dsaparams],
                               stdout => "err.ss")),
                       "make a DSA key");
                skip 'failure', 3 unless
                    ok(run(app([@reqcmd, "-new", "-config", $Uconf,
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
                                "-extfile", $Uconf,
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
                    ok(run(app(["openssl", "ecparam", "-name", "P-256",
                                "-out", "ecp.ss"])),
                       "make EC parameters");
                skip 'failure', 3 unless
                    ok(run(app([@reqcmd, "-config", $Uconf,
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
                                "-extfile", $Uconf,
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
	  ok(run(app([@reqcmd, "-config", $P1conf,
		      "-out", $P1req, "-keyout", $P1key, @req_new],
		     stdout => "err.ss")),
	     'make a proxy cert request');


      skip 'failure', 4 unless
	  ok(run(app([@x509cmd, "-CAcreateserial", "-in", $P1req, "-days", "30",
		      "-req", "-out", $P1cert,
		      "-CA", $Ucert, "-CAkey", $Ukey,
		      "-extfile", $P1conf, "-extensions", "v3_proxy"],
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
	  ok(run(app([@reqcmd, "-config", $P2conf,
		      "-out", $P2req, "-keyout", $P2key,
		      @req_new],
		     stdout => "err.ss")),
	     'make another proxy cert request');


      skip 'failure', 1 unless
	  ok(run(app([@x509cmd, "-CAcreateserial", "-in", $P2req, "-days", "30",
		      "-req", "-out", $P2cert,
		      "-CA", $P1cert, "-CAkey", $P1key,
		      "-extfile", $P2conf, "-extensions", "v3_proxy"],
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
    my ($key, $cert, $CAtmp) = @_;
    my @CA = $CAtmp ? ("-CAfile", $CAtmp) : ("-CApath", bldtop_dir("certs"));

    my @ssltest = ("ssltest_old",
		   "-s_key", $key, "-s_cert", $cert,
		   "-c_key", $key, "-c_cert", $cert);

    my $serverinfo = srctop_file("test","serverinfo.pem");

    my $dsa_cert = 0;
    if (grep /DSA Public Key/, run(app(["openssl", "x509", "-in", $cert,
					"-text", "-noout"]), capture => 1)) {
	$dsa_cert = 1;
    }


    # plan tests => 11;

    subtest 'standard SSL tests' => sub {
	######################################################################
      plan tests => 13;

      SKIP: {
	  skip "SSLv3 is not supported by this OpenSSL build", 4
	      if disabled("ssl3");

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
	  skip "Neither SSLv3 nor any TLS version are supported by this OpenSSL build", 8
	      if $no_anytls;

	SKIP: {
	    skip "skipping test of sslv2/sslv3 w/o (EC)DHE test", 1 if $dsa_cert;

	    ok(run(test([@ssltest, "-bio_pair", "-no_dhe", "-no_ecdhe"])),
	       'test sslv2/sslv3 w/o (EC)DHE via BIO pair');
	  }

	  ok(run(test([@ssltest, "-bio_pair", "-dhe1024dsa", "-v"])),
	     'test sslv2/sslv3 with 1024bit DHE via BIO pair');
	  ok(run(test([@ssltest, "-bio_pair", "-server_auth", @CA])),
	     'test sslv2/sslv3 with server authentication');
	  ok(run(test([@ssltest, "-bio_pair", "-client_auth", @CA])),
	     'test sslv2/sslv3 with client authentication via BIO pair');
	  ok(run(test([@ssltest, "-bio_pair", "-server_auth", "-client_auth", @CA])),
	     'test sslv2/sslv3 with both client and server authentication via BIO pair');
	  ok(run(test([@ssltest, "-bio_pair", "-server_auth", "-client_auth", "-app_verify", @CA])),
	     'test sslv2/sslv3 with both client and server authentication via BIO pair and app verify');

        SKIP: {
            skip "No IPv4 available on this machine", 1
                unless !disabled("sock") && have_IPv4();
            ok(run(test([@ssltest, "-ipv4"])),
               'test TLS via IPv4');
          }

        SKIP: {
            skip "No IPv6 available on this machine", 1
                unless !disabled("sock") && have_IPv6();
            ok(run(test([@ssltest, "-ipv6"])),
               'test TLS via IPv6');
          }
        }
    };

    subtest "Testing ciphersuites" => sub {

        my @exkeys = ();
        my $ciphers = "-PSK:-SRP";

        if (!$no_dsa) {
            push @exkeys, "-s_cert", "certD.ss", "-s_key", "keyD.ss";
        }

        if (!$no_ec) {
            push @exkeys, "-s_cert", "certE.ss", "-s_key", "keyE.ss";
        }

	my @protocols = ();
	# We only use the flags that ssltest_old understands
	push @protocols, "-tls1_3" unless $no_tls1_3;
	push @protocols, "-tls1_2" unless $no_tls1_2;
	push @protocols, "-tls1" unless $no_tls1;
	push @protocols, "-ssl3" unless $no_ssl3;
	my $protocolciphersuitecount = 0;
	my %ciphersuites = ();
	my %ciphersstatus = ();
	foreach my $protocol (@protocols) {
	    my $ciphersstatus = undef;
	    my @ciphers = run(app(["openssl", "ciphers", "-s", $protocol,
				   "ALL:$ciphers"]),
			      capture => 1, statusvar => \$ciphersstatus);
	    $ciphersstatus{$protocol} = $ciphersstatus;
	    if ($ciphersstatus) {
		$ciphersuites{$protocol} = [ map { s|\R||; split(/:/, $_) }
					     @ciphers ];
		$protocolciphersuitecount += scalar @{$ciphersuites{$protocol}};
	    }
	}

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
            # ssltest_old doesn't know -tls1_3, but that's fine, since that's
            # the default choice if TLSv1.3 enabled
            my $flag = $protocol eq "-tls1_3" ? "" : $protocol;
            my $ciphersuites = "";
            foreach my $cipher (@{$ciphersuites{$protocol}}) {
                if ($protocol eq "-ssl3" && $cipher =~ /ECDH/ ) {
                    note "*****SKIPPING $protocol $cipher";
                    ok(1);
                } else {
                    if ($protocol eq "-tls1_3") {
                        $ciphersuites = $cipher;
                        $cipher = "";
                    }
                    ok(run(test([@ssltest, @exkeys, "-cipher", $cipher,
                                 "-ciphersuites", $ciphersuites, $flag || ()])),
                       "Testing $cipher");
                }
            }
            next if $protocol eq "-tls1_3";
            is(run(test([@ssltest,
                         "-s_cipher", "EDH",
                         "-c_cipher", 'EDH:@SECLEVEL=1',
                         "-dhe512",
                         $protocol])), 0,
               "testing connection with weak DH, expecting failure");
        }
    };

    subtest 'RSA/(EC)DHE/PSK tests' => sub {
	######################################################################

	plan tests => 5;

      SKIP: {
	  skip "TLSv1.0 is not supported by this OpenSSL build", 5
	      if $no_tls1;

	SKIP: {
	    skip "skipping anonymous DH tests", 1
	      if ($no_dh);

	    ok(run(test([@ssltest, "-v", "-bio_pair", "-tls1", "-cipher", "ADH", "-dhe1024dsa", "-num", "10", "-f", "-time"])),
	       'test tlsv1 with 1024bit anonymous DH, multiple handshakes');
	  }

	SKIP: {
	    skip "skipping RSA tests", 2
		if $no_rsa;

	    ok(run(test(["ssltest_old", "-v", "-bio_pair", "-tls1", "-s_cert", srctop_file("apps","server2.pem"), "-no_dhe", "-no_ecdhe", "-num", "10", "-f", "-time"])),
	       'test tlsv1 with 1024bit RSA, no (EC)DHE, multiple handshakes');

	    skip "skipping RSA+DHE tests", 1
		if $no_dh;

	    ok(run(test(["ssltest_old", "-v", "-bio_pair", "-tls1", "-s_cert", srctop_file("apps","server2.pem"), "-dhe1024dsa", "-num", "10", "-f", "-time"])),
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
	}

    };

    subtest 'Custom Extension tests' => sub {
	######################################################################

	plan tests => 1;

      SKIP: {
	  skip "TLSv1.0 is not supported by this OpenSSL build", 1
	      if $no_tls1;

	  ok(run(test([@ssltest, "-bio_pair", "-tls1", "-custom_ext"])),
	     'test tls1 with custom extensions');
	}
    };

    subtest 'Serverinfo tests' => sub {
	######################################################################

	plan tests => 5;

      SKIP: {
	  skip "TLSv1.0 is not supported by this OpenSSL build", 5
	      if $no_tls1;

	  note('echo test tls1 with serverinfo');
	  ok(run(test([@ssltest, "-bio_pair", "-tls1", "-serverinfo_file", $serverinfo])));
	  ok(run(test([@ssltest, "-bio_pair", "-tls1", "-serverinfo_file", $serverinfo, "-serverinfo_sct"])));
	  ok(run(test([@ssltest, "-bio_pair", "-tls1", "-serverinfo_file", $serverinfo, "-serverinfo_tack"])));
	  ok(run(test([@ssltest, "-bio_pair", "-tls1", "-serverinfo_file", $serverinfo, "-serverinfo_sct", "-serverinfo_tack"])));
	  ok(run(test([@ssltest, "-bio_pair", "-tls1", "-custom_ext", "-serverinfo_file", $serverinfo, "-serverinfo_sct", "-serverinfo_tack"])));
	}
    };
}

unlink $CAkey;
unlink $CAcert;
unlink $CAserial;
unlink $CAreq;
unlink $CAreq2;

unlink $Ukey;
unlink $Ureq;
unlink $Ucert;
unlink basename($Ucert, '.ss').'.srl';

unlink $Dkey;
unlink $Dreq;
unlink $Dcert;

unlink $Ekey;
unlink $Ereq;
unlink $Ecert;

unlink $P1key;
unlink $P1req;
unlink $P1cert;
unlink basename($P1cert, '.ss').'.srl';
unlink $P1intermediate;
unlink "intP1.ss";

unlink $P2key;
unlink $P2req;
unlink $P2cert;
unlink $P2intermediate;
unlink "intP2.ss";

unlink "ecp.ss";
unlink "err.ss";

unlink $server_sess;
unlink $client_sess;
