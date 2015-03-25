#!/usr/bin/perl -w
# ***************************************************************************
# *                                  _   _ ____  _
# *  Project                     ___| | | |  _ \| |
# *                             / __| | | | |_) | |
# *                            | (__| |_| |  _ <| |___
# *                             \___|\___/|_| \_\_____|
# *
# * Copyright (C) 1998 - 2014, Daniel Stenberg, <daniel@haxx.se>, et al.
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
use vars qw($opt_b $opt_d $opt_f $opt_h $opt_i $opt_l $opt_n $opt_p $opt_q $opt_s $opt_t $opt_u $opt_v $opt_w);
use List::Util;
use Text::Wrap;
my $MOD_SHA = "Digest::SHA";
eval "require $MOD_SHA";
if ($@) {
  $MOD_SHA = "Digest::SHA::PurePerl";
  eval "require $MOD_SHA";
}

my %urls = (
  'nss' =>
    'http://hg.mozilla.org/projects/nss/raw-file/tip/lib/ckfw/builtins/certdata.txt',
  'central' =>
    'http://hg.mozilla.org/mozilla-central/raw-file/default/security/nss/lib/ckfw/builtins/certdata.txt',
  'aurora' =>
    'http://hg.mozilla.org/releases/mozilla-aurora/raw-file/default/security/nss/lib/ckfw/builtins/certdata.txt',
  'beta' =>
    'http://hg.mozilla.org/releases/mozilla-beta/raw-file/default/security/nss/lib/ckfw/builtins/certdata.txt',
  'release' =>
    'http://hg.mozilla.org/releases/mozilla-release/raw-file/default/security/nss/lib/ckfw/builtins/certdata.txt',
);

$opt_d = 'release';

# If the OpenSSL commandline is not in search path you can configure it here!
my $openssl = 'openssl';

my $version = '1.25';

$opt_w = 76; # default base64 encoded lines length

# default cert types to include in the output (default is to include CAs which may issue SSL server certs)
my $default_mozilla_trust_purposes = "SERVER_AUTH";
my $default_mozilla_trust_levels = "TRUSTED_DELEGATOR";
$opt_p = $default_mozilla_trust_purposes . ":" . $default_mozilla_trust_levels;

my @valid_mozilla_trust_purposes = (
  "DIGITAL_SIGNATURE",
  "NON_REPUDIATION",
  "KEY_ENCIPHERMENT",
  "DATA_ENCIPHERMENT",
  "KEY_AGREEMENT",
  "KEY_CERT_SIGN",
  "CRL_SIGN",
  "SERVER_AUTH",
  "CLIENT_AUTH",
  "CODE_SIGNING",
  "EMAIL_PROTECTION",
  "IPSEC_END_SYSTEM",
  "IPSEC_TUNNEL",
  "IPSEC_USER",
  "TIME_STAMPING",
  "STEP_UP_APPROVED"
);

my @valid_mozilla_trust_levels = (
  "TRUSTED_DELEGATOR",    # CAs
  "NOT_TRUSTED",          # Don't trust these certs.
  "MUST_VERIFY_TRUST",    # This explicitly tells us that it ISN'T a CA but is otherwise ok. In other words, this should tell the app to ignore any other sources that claim this is a CA.
  "TRUSTED"               # This cert is trusted, but only for itself and not for delegates (i.e. it is not a CA).
);

my $default_signature_algorithms = $opt_s = "MD5";

my @valid_signature_algorithms = (
  "MD5",
  "SHA1",
  "SHA256",
  "SHA384",
  "SHA512"
);

$0 =~ s@.*(/|\\)@@;
$Getopt::Std::STANDARD_HELP_VERSION = 1;
getopts('bd:fhilnp:qs:tuvw:');

if(!defined($opt_d)) {
    # to make plain "-d" use not cause warnings, and actually still work
    $opt_d = 'release';
}

# Use predefined URL or else custom URL specified on command line.
my $url = ( defined( $urls{$opt_d} ) ) ? $urls{$opt_d} : $opt_d;

my $curl = `curl -V`;

if ($opt_i) {
  print ("=" x 78 . "\n");
  print "Script Version                   : $version\n";
  print "Perl Version                     : $]\n";
  print "Operating System Name            : $^O\n";
  print "Getopt::Std.pm Version           : ${Getopt::Std::VERSION}\n";
  print "MIME::Base64.pm Version          : ${MIME::Base64::VERSION}\n";
  print "LWP::UserAgent.pm Version        : ${LWP::UserAgent::VERSION}\n";
  print "LWP.pm Version                   : ${LWP::VERSION}\n";
  print "Digest::SHA.pm Version           : ${Digest::SHA::VERSION}\n" if ($Digest::SHA::VERSION);
  print "Digest::SHA::PurePerl.pm Version : ${Digest::SHA::PurePerl::VERSION}\n" if ($Digest::SHA::PurePerl::VERSION);
  print ("=" x 78 . "\n");
}

sub warning_message() {
  if ( $opt_d =~ m/^risk$/i ) { # Long Form Warning and Exit
    print "Warning: Use of this script may pose some risk:\n";
    print "\n";
    print "  1) Using http is subject to man in the middle attack of certdata content\n";
    print "  2) Default to 'release', but more recent updates may be found in other trees\n";
    print "  3) certdata.txt file format may change, lag time to update this script\n";
    print "  4) Generally unwise to blindly trust CAs without manual review & verification\n";
    print "  5) Mozilla apps use additional security checks aren't represented in certdata\n";
    print "  6) Use of this script will make a security engineer grind his teeth and\n";
    print "     swear at you.  ;)\n";
    exit;
  } else { # Short Form Warning
    print "Warning: Use of this script may pose some risk, -d risk for more details.\n";
  }
}

sub HELP_MESSAGE() {
  print "Usage:\t${0} [-b] [-d<certdata>] [-f] [-i] [-l] [-n] [-p<purposes:levels>] [-q] [-s<algorithms>] [-t] [-u] [-v] [-w<l>] [<outputfile>]\n";
  print "\t-b\tbackup an existing version of ca-bundle.crt\n";
  print "\t-d\tspecify Mozilla tree to pull certdata.txt or custom URL\n";
  print "\t\t  Valid names are:\n";
  print "\t\t    ", join( ", ", map { ( $_ =~ m/$opt_d/ ) ? "$_ (default)" : "$_" } sort keys %urls ), "\n";
  print "\t-f\tforce rebuild even if certdata.txt is current\n";
  print "\t-i\tprint version info about used modules\n";
  print "\t-l\tprint license info about certdata.txt\n";
  print "\t-n\tno download of certdata.txt (to use existing)\n";
  print wrap("\t","\t\t", "-p\tlist of Mozilla trust purposes and levels for certificates to include in output. Takes the form of a comma separated list of purposes, a colon, and a comma separated list of levels. (default: $default_mozilla_trust_purposes:$default_mozilla_trust_levels)"), "\n";
  print "\t\t  Valid purposes are:\n";
  print wrap("\t\t    ","\t\t    ", join( ", ", "ALL", @valid_mozilla_trust_purposes ) ), "\n";
  print "\t\t  Valid levels are:\n";
  print wrap("\t\t    ","\t\t    ", join( ", ", "ALL", @valid_mozilla_trust_levels ) ), "\n";
  print "\t-q\tbe really quiet (no progress output at all)\n";
  print wrap("\t","\t\t", "-s\tcomma separated list of certificate signatures/hashes to output in plain text mode. (default: $default_signature_algorithms)\n");
  print "\t\t  Valid signature algorithms are:\n";
  print wrap("\t\t    ","\t\t    ", join( ", ", "ALL", @valid_signature_algorithms ) ), "\n";
  print "\t-t\tinclude plain text listing of certificates\n";
  print "\t-u\tunlink (remove) certdata.txt after processing\n";
  print "\t-v\tbe verbose and print out processed CAs\n";
  print "\t-w <l>\twrap base64 output lines after <l> chars (default: ${opt_w})\n";
  exit;
}

sub VERSION_MESSAGE() {
  print "${0} version ${version} running Perl ${]} on ${^O}\n";
}

warning_message() unless ($opt_q || $url =~ m/^(ht|f)tps:/i );
HELP_MESSAGE() if ($opt_h);

sub report($@) {
  my $output = shift;

  print STDERR $output . "\n" unless $opt_q;
}

sub is_in_list($@) {
  my $target = shift;

  return defined(List::Util::first { $target eq $_ } @_);
}

# Parses $param_string as a case insensitive comma separated list with optional whitespace
# validates that only allowed parameters are supplied
sub parse_csv_param($$@) {
  my $description = shift;
  my $param_string = shift;
  my @valid_values = @_;

  my @values = map {
    s/^\s+//;  # strip leading spaces
    s/\s+$//;  # strip trailing spaces
    uc $_      # return the modified string as upper case
  } split( ',', $param_string );

  # Find all values which are not in the list of valid values or "ALL"
  my @invalid = grep { !is_in_list($_,"ALL",@valid_values) } @values;

  if ( scalar(@invalid) > 0 ) {
    # Tell the user which parameters were invalid and print the standard help message which will exit
    print "Error: Invalid ", $description, scalar(@invalid) == 1 ? ": " : "s: ", join( ", ", map { "\"$_\"" } @invalid ), "\n";
    HELP_MESSAGE();
  }

  @values = @valid_values if ( is_in_list("ALL",@values) );

  return @values;
}

sub sha1 {
  my $result;
  if ($Digest::SHA::VERSION || $Digest::SHA::PurePerl::VERSION) {
    open(FILE, $_[0]) or die "Can't open '$_[0]': $!";
    binmode(FILE);
    $result = $MOD_SHA->new(1)->addfile(*FILE)->hexdigest;
    close(FILE);
  } else {
    # Use OpenSSL command if Perl Digest::SHA modules not available
    $result = (split(/ |\r|\n/,`$openssl dgst -sha1 $_[0]`))[1];
  }
  return $result;
}


sub oldsha1 {
  my $sha1 = "";
  open(C, "<$_[0]") || return 0;
  while(<C>) {
    chomp;
    if($_ =~ /^\#\# SHA1: (.*)/) {
      $sha1 = $1;
      last;
    }
  }
  close(C);
  return $sha1;
}

if ( $opt_p !~ m/:/ ) {
  print "Error: Mozilla trust identifier list must include both purposes and levels\n";
  HELP_MESSAGE();
}

(my $included_mozilla_trust_purposes_string, my $included_mozilla_trust_levels_string) = split( ':', $opt_p );
my @included_mozilla_trust_purposes = parse_csv_param( "trust purpose", $included_mozilla_trust_purposes_string, @valid_mozilla_trust_purposes );
my @included_mozilla_trust_levels = parse_csv_param( "trust level", $included_mozilla_trust_levels_string, @valid_mozilla_trust_levels );

my @included_signature_algorithms = parse_csv_param( "signature algorithm", $opt_s, @valid_signature_algorithms );

sub should_output_cert(%) {
  my %trust_purposes_by_level = @_;

  foreach my $level (@included_mozilla_trust_levels) {
    # for each level we want to output, see if any of our desired purposes are included
    return 1 if ( defined( List::Util::first { is_in_list( $_, @included_mozilla_trust_purposes ) } @{$trust_purposes_by_level{$level}} ) );
  }

  return 0;
}

my $crt = $ARGV[0] || 'ca-bundle.crt';
(my $txt = $url) =~ s@(.*/|\?.*)@@g;

my $stdout = $crt eq '-';
my $resp;
my $fetched;

my $oldsha1 = oldsha1($crt);

report "SHA1 of old file: $oldsha1";

report "Downloading '$txt' ...";

if($curl && !$opt_n) {
  my $https = $url;
  $https =~ s/^http:/https:/;
  report "Get certdata over HTTPS with curl!";
  my $quiet = $opt_q ? "-s" : "";
  my @out = `curl -w %{response_code} $quiet -O $https`;
  if(@out && $out[0] == 200) {
    $fetched = 1;
  } else {
    report "Failed downloading HTTPS with curl, trying HTTP with LWP";
  }
}

unless ($fetched || ($opt_n and -e $txt)) {
  my $ua  = new LWP::UserAgent(agent => "$0/$version");
  $ua->env_proxy();
  $resp = $ua->mirror($url, $txt);
  if ($resp && $resp->code eq '304') {
    report "Not modified";
    exit 0 if -e $crt && !$opt_f;
  } else {
      $fetched = 1;
  }
  if( !$resp || $resp->code !~ /^(?:200|304)$/ ) {
      report "Unable to download latest data: "
        . ($resp? $resp->code . ' - ' . $resp->message : "LWP failed");
      exit 1 if -e $crt || ! -r $txt;
  }
}

my $filedate = $resp ? $resp->last_modified : (stat($txt))[9];
my $datesrc = "as of";
if(!$filedate) {
    # mxr.mozilla.org gave us a time, hg.mozilla.org does not!
    $filedate = time();
    $datesrc="downloaded on";
}

# get the hash from the download file
my $newsha1= sha1($txt);

if(!$opt_f && $oldsha1 eq $newsha1) {
    report "Downloaded file identical to previous run\'s source file. Exiting";
    exit;
}

report "SHA1 of new file: $newsha1";

my $currentdate = scalar gmtime($filedate);

my $format = $opt_t ? "plain text and " : "";
if( $stdout ) {
    open(CRT, '> -') or die "Couldn't open STDOUT: $!\n";
} else {
    open(CRT,">$crt.~") or die "Couldn't open $crt.~: $!\n";
}
print CRT <<EOT;
##
## Bundle of CA Root Certificates
##
## Certificate data from Mozilla ${datesrc}: ${currentdate}
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
## Conversion done with mk-ca-bundle.pl version $version.
## SHA1: $newsha1
##

EOT

report "Processing  '$txt' ...";
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
  my %trust_purposes_by_level;
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
    # now scan the trust part to determine how we should trust this cert
    while (<TXT>) {
      last if (/^#/);
      if (/^CKA_TRUST_([A-Z_]+)\s+CK_TRUST\s+CKT_NSS_([A-Z_]+)\s*$/) {
        if ( !is_in_list($1,@valid_mozilla_trust_purposes) ) {
          report "Warning: Unrecognized trust purpose for cert: $caname. Trust purpose: $1. Trust Level: $2";
        } elsif ( !is_in_list($2,@valid_mozilla_trust_levels) ) {
          report "Warning: Unrecognized trust level for cert: $caname. Trust purpose: $1. Trust Level: $2";
        } else {
          push @{$trust_purposes_by_level{$2}}, $1;
        }
      }
    }

    if ( !should_output_cert(%trust_purposes_by_level) ) {
      $skipnum ++;
    } else {
      my $encoded = MIME::Base64::encode_base64($data, '');
      $encoded =~ s/(.{1,${opt_w}})/$1\n/g;
      my $pem = "-----BEGIN CERTIFICATE-----\n"
              . $encoded
              . "-----END CERTIFICATE-----\n";
      print CRT "\n$caname\n";

      my $maxStringLength = length($caname);
      if ($opt_t) {
        foreach my $key (keys %trust_purposes_by_level) {
           my $string = $key . ": " . join(", ", @{$trust_purposes_by_level{$key}});
           $maxStringLength = List::Util::max( length($string), $maxStringLength );
           print CRT $string . "\n";
        }
      }
      print CRT ("=" x $maxStringLength . "\n");
      if (!$opt_t) {
        print CRT $pem;
      } else {
        my $pipe = "";
        foreach my $hash (@included_signature_algorithms) {
          $pipe = "|$openssl x509 -" . $hash . " -fingerprint -noout -inform PEM";
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
        $pipe = "|$openssl x509 -text -inform PEM";
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
      report "Parsing: $caname" if ($opt_v);
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
report "Done ($certnum CA certs processed, $skipnum skipped).";
