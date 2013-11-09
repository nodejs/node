#!/usr/bin/perl -w
# ***************************************************************************
# *                                  _   _ ____  _
# *  Project                     ___| | | |  _ \| |
# *                             / __| | | | |_) | |
# *                            | (__| |_| |  _ <| |___
# *                             \___|\___/|_| \_\_____|
# *
# * Copyright (C) 1998 - 2013, Daniel Stenberg, <daniel@haxx.se>, et al.
# *
# * This software is licensed as described in the file COPYING, which
# * you should have received as part of this distribution. The terms
# * are also available at http://curl.haxx.se/docs/copyright.html.
# *
# * You may opt to use, copy, modify, merge, publish, distribute and/or sell
# * copies of the Software, and permit persons to whom the Software is
# * furnished to do so, under the terms of the COPYING file.
# *
# * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# * KIND, either express or implied.
# *
# ***************************************************************************
# This Perl script creates a fresh ca-bundle.crt file for use with libcurl.
# It downloads certdata.txt from Mozilla's source tree (see URL below),
# then parses certdata.txt and extracts CA Root Certificates into PEM format.
# These are then processed with the OpenSSL commandline tool to produce the
# final ca-bundle.crt file.
# The script is based on the parse-certs script written by Roland Krikava.
# This Perl script works on almost any platform since its only external
# dependency is the OpenSSL commandline tool for optional text listing.
# Hacked by Guenter Knauf.
#
use Getopt::Std;
use MIME::Base64;
use LWP::UserAgent;
use strict;
use vars qw($opt_b $opt_f $opt_h $opt_i $opt_l $opt_n $opt_q $opt_t $opt_u $opt_v $opt_w);

my $url = 'http://mxr.mozilla.org/mozilla/source/security/nss/lib/ckfw/builtins/certdata.txt?raw=1';
# If the OpenSSL commandline is not in search path you can configure it here!
my $openssl = 'openssl';

my $version = '1.19';

$opt_w = 76; # default base64 encoded lines length

$0 =~ s@.*(/|\\)@@;
$Getopt::Std::STANDARD_HELP_VERSION = 1;
getopts('bfhilnqtuvw:');

if ($opt_i) {
  print ("=" x 78 . "\n");
  print "Script Version            : $version\n";
  print "Perl Version              : $]\n";
  print "Operating System Name     : $^O\n";
  print "Getopt::Std.pm Version    : ${Getopt::Std::VERSION}\n";
  print "MIME::Base64.pm Version   : ${MIME::Base64::VERSION}\n";
  print "LWP::UserAgent.pm Version : ${LWP::UserAgent::VERSION}\n";
  print "LWP.pm Version            : ${LWP::VERSION}\n";
  print ("=" x 78 . "\n");
}

sub HELP_MESSAGE() {
  print "Usage:\t${0} [-b] [-f] [-i] [-l] [-n] [-q] [-t] [-u] [-v] [-w<l>] [<outputfile>]\n";
  print "\t-b\tbackup an existing version of ca-bundle.crt\n";
  print "\t-f\tforce rebuild even if certdata.txt is current\n";
  print "\t-i\tprint version info about used modules\n";
  print "\t-l\tprint license info about certdata.txt\n";
  print "\t-n\tno download of certdata.txt (to use existing)\n";
  print "\t-q\tbe really quiet (no progress output at all)\n";
  print "\t-t\tinclude plain text listing of certificates\n";
  print "\t-u\tunlink (remove) certdata.txt after processing\n";
  print "\t-v\tbe verbose and print out processed CAs\n";
  print "\t-w <l>\twrap base64 output lines after <l> chars (default: ${opt_w})\n";
  exit;
}

sub VERSION_MESSAGE() {
  print "${0} version ${version} running Perl ${]} on ${^O}\n";
}

HELP_MESSAGE() if ($opt_h);

my $crt = $ARGV[0] || 'ca-bundle.crt';
(my $txt = $url) =~ s@(.*/|\?.*)@@g;

my $stdout = $crt eq '-';
my $resp;
my $fetched;

unless ($opt_n and -e $txt) {
  print STDERR "Downloading '$txt' ...\n" if (!$opt_q);
  my $ua  = new LWP::UserAgent(agent => "$0/$version");
  $ua->env_proxy();
  $resp = $ua->mirror($url, $txt);
  if ($resp && $resp->code eq '304') {
    print STDERR "Not modified\n" unless $opt_q;
    exit 0 if -e $crt && !$opt_f;
  } else {
      $fetched = 1;
  }
  if( !$resp || $resp->code !~ /^(?:200|304)$/ ) {
      print STDERR "Unable to download latest data: "
        . ($resp? $resp->code . ' - ' . $resp->message : "LWP failed") . "\n"
        unless $opt_q;
      exit 1 if -e $crt || ! -r $txt;
  }
}

my $currentdate = scalar gmtime($fetched ? $resp->last_modified : (stat($txt))[9]);

my $format = $opt_t ? "plain text and " : "";
if( $stdout ) {
    open(CRT, '> -') or die "Couldn't open STDOUT: $!\n";
} else {
    open(CRT,">$crt.~") or die "Couldn't open $crt.~: $!\n";
}
print CRT <<EOT;
##
## $crt -- Bundle of CA Root Certificates
##
## Certificate data from Mozilla as of: ${currentdate}
##
## This is a bundle of X.509 certificates of public Certificate Authorities
## (CA). These were automatically extracted from Mozilla's root certificates
## file (certdata.txt).  This file can be found in the mozilla source tree:
## ${url}
##
## It contains the certificates in ${format}PEM format and therefore
## can be directly used with curl / libcurl / php_curl, or with
## an Apache+mod_ssl webserver for SSL client authentication.
## Just configure this file as the SSLCACertificateFile.
##

EOT

print STDERR "Processing  '$txt' ...\n" if (!$opt_q);
my $caname;
my $certnum = 0;
my $skipnum = 0;
my $start_of_cert = 0;

open(TXT,"$txt") or die "Couldn't open $txt: $!\n";
while (<TXT>) {
  if (/\*\*\*\*\* BEGIN LICENSE BLOCK \*\*\*\*\*/) {
    print CRT;
    print if ($opt_l);
    while (<TXT>) {
      print CRT;
      print if ($opt_l);
      last if (/\*\*\*\*\* END LICENSE BLOCK \*\*\*\*\*/);
    }
  }
  next if /^#|^\s*$/;
  chomp;
  if (/^CVS_ID\s+\"(.*)\"/) {
    print CRT "# $1\n";
  }

  # this is a match for the start of a certificate
  if (/^CKA_CLASS CK_OBJECT_CLASS CKO_CERTIFICATE/) {
    $start_of_cert = 1
  }
  if ($start_of_cert && /^CKA_LABEL UTF8 \"(.*)\"/) {
    $caname = $1;
  }
  my $untrusted = 1;
  if ($start_of_cert && /^CKA_VALUE MULTILINE_OCTAL/) {
    my $data;
    while (<TXT>) {
      last if (/^END/);
      chomp;
      my @octets = split(/\\/);
      shift @octets;
      for (@octets) {
        $data .= chr(oct);
      }
    }
    # scan forwards until the trust part
    while (<TXT>) {
      last if (/^CKA_CLASS CK_OBJECT_CLASS CKO_NSS_TRUST/);
      chomp;
    }
    # now scan the trust part for untrusted certs
    while (<TXT>) {
      last if (/^#/);
      if (/^CKA_TRUST_SERVER_AUTH\s+CK_TRUST\s+CKT_NSS_TRUSTED_DELEGATOR$/) {
          $untrusted = 0;
      }
    }
    if ($untrusted) {
      $skipnum ++;
    } else {
      my $encoded = MIME::Base64::encode_base64($data, '');
      $encoded =~ s/(.{1,${opt_w}})/$1\n/g;
      my $pem = "-----BEGIN CERTIFICATE-----\n"
              . $encoded
              . "-----END CERTIFICATE-----\n";
      print CRT "\n$caname\n";
      print CRT ("=" x length($caname) . "\n");
      if (!$opt_t) {
        print CRT $pem;
      } else {
        my $pipe = "|$openssl x509 -md5 -fingerprint -text -inform PEM";
        if (!$stdout) {
          $pipe .= " >> $crt.~";
          close(CRT) or die "Couldn't close $crt.~: $!";
        }
        open(TMP, $pipe) or die "Couldn't open openssl pipe: $!";
        print TMP $pem;
        close(TMP) or die "Couldn't close openssl pipe: $!";
        if (!$stdout) {
          open(CRT, ">>$crt.~") or die "Couldn't open $crt.~: $!";
        }
      }
      print STDERR "Parsing: $caname\n" if ($opt_v);
      $certnum ++;
      $start_of_cert = 0;
    }
  }
}
close(TXT) or die "Couldn't close $txt: $!\n";
close(CRT) or die "Couldn't close $crt.~: $!\n";
unless( $stdout ) {
    if ($opt_b && -e $crt) {
        my $bk = 1;
        while (-e "$crt.~${bk}~") {
            $bk++;
        }
        rename $crt, "$crt.~${bk}~" or die "Failed to create backup $crt.~$bk}~: $!\n";
    } elsif( -e $crt ) {
        unlink( $crt ) or die "Failed to remove $crt: $!\n";
    }
    rename "$crt.~", $crt or die "Failed to rename $crt.~ to $crt: $!\n";
}
unlink $txt if ($opt_u);
print STDERR "Done ($certnum CA certs processed, $skipnum untrusted skipped).\n" if (!$opt_q);

exit;


