#!/usr/bin/env perl
# ***************************************************************************
# *                                  _   _ ____  _
# *  Project                     ___| | | |  _ \| |
# *                             / __| | | | |_) | |
# *                            | (__| |_| |  _ <| |___
# *                             \___|\___/|_| \_\_____|
# *
# * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
# *
# * This software is licensed as described in the file COPYING, which
# * you should have received as part of this distribution. The terms
# * are also available at https://curl.se/docs/copyright.html.
# *
# * You may opt to use, copy, modify, merge, publish, distribute and/or sell
# * copies of the Software, and permit persons to whom the Software is
# * furnished to do so, under the terms of the COPYING file.
# *
# * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# * KIND, either express or implied.
# *
# * SPDX-License-Identifier: curl
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
use Encode;
use Getopt::Std;
use MIME::Base64;
use strict;
use warnings;
use vars qw($opt_b $opt_d $opt_f $opt_h $opt_i $opt_k $opt_l $opt_m $opt_n $opt_p $opt_q $opt_s $opt_t $opt_u $opt_v $opt_w);
use List::Util;
use Text::Wrap;
use Time::Local;
my $MOD_SHA = "Digest::SHA";
eval "require $MOD_SHA";
if ($@) {
  $MOD_SHA = "Digest::SHA::PurePerl";
  eval "require $MOD_SHA";
}
eval "require LWP::UserAgent";

my %urls = (
  'nss' =>
    'https://hg.mozilla.org/projects/nss/raw-file/default/lib/ckfw/builtins/certdata.txt',
  'central' =>
    'https://hg.mozilla.org/mozilla-central/raw-file/default/security/nss/lib/ckfw/builtins/certdata.txt',
  'beta' =>
    'https://hg.mozilla.org/releases/mozilla-beta/raw-file/default/security/nss/lib/ckfw/builtins/certdata.txt',
  'release' =>
    'https://hg.mozilla.org/releases/mozilla-release/raw-file/default/security/nss/lib/ckfw/builtins/certdata.txt',
);

$opt_d = 'release';

# If the OpenSSL commandline is not in search path you can configure it here!
my $openssl = 'openssl';

my $version = '1.29';

$opt_w = 76; # default base64 encoded lines length

# default cert types to include in the output (default is to include CAs which
# may issue SSL server certs)
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
  "MUST_VERIFY_TRUST",    # This explicitly tells us that it ISN'T a CA but is
                          # otherwise ok. In other words, this should tell the
                          # app to ignore any other sources that claim this is
                          # a CA.
  "TRUSTED"               # This cert is trusted, but only for itself and not
                          # for delegates (i.e. it is not a CA).
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
getopts('bd:fhiklmnp:qs:tuvw:');

if(!defined($opt_d)) {
    # to make plain "-d" use not cause warnings, and actually still work
    $opt_d = 'release';
}

# Use predefined URL or else custom URL specified on command line.
my $url;
if(defined($urls{$opt_d})) {
  $url = $urls{$opt_d};
  if(!$opt_k && $url !~ /^https:\/\//i) {
    die "The URL for '$opt_d' is not HTTPS. Use -k to override (insecure).\n";
  }
}
else {
  $url = $opt_d;
}

if ($opt_i) {
  print ("=" x 78 . "\n");
  print "Script Version                   : $version\n";
  print "Perl Version                     : $]\n";
  print "Operating System Name            : $^O\n";
  print "Getopt::Std.pm Version           : ${Getopt::Std::VERSION}\n";
  print "Encode::Encoding.pm Version      : ${Encode::Encoding::VERSION}\n";
  print "MIME::Base64.pm Version          : ${MIME::Base64::VERSION}\n";
  print "LWP::UserAgent.pm Version        : ${LWP::UserAgent::VERSION}\n" if($LWP::UserAgent::VERSION);
  print "LWP.pm Version                   : ${LWP::VERSION}\n" if($LWP::VERSION);
  print "Digest::SHA.pm Version           : ${Digest::SHA::VERSION}\n" if ($Digest::SHA::VERSION);
  print "Digest::SHA::PurePerl.pm Version : ${Digest::SHA::PurePerl::VERSION}\n" if ($Digest::SHA::PurePerl::VERSION);
  print ("=" x 78 . "\n");
}

sub warning_message() {
  if ( $opt_d =~ m/^risk$/i ) { # Long Form Warning and Exit
    print "Warning: Use of this script may pose some risk:\n";
    print "\n";
    print "  1) If you use HTTP URLs they are subject to a man in the middle attack\n";
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
  print "Usage:\t${0} [-b] [-d<certdata>] [-f] [-i] [-k] [-l] [-n] [-p<purposes:levels>] [-q] [-s<algorithms>] [-t] [-u] [-v] [-w<l>] [<outputfile>]\n";
  print "\t-b\tbackup an existing version of ca-bundle.crt\n";
  print "\t-d\tspecify Mozilla tree to pull certdata.txt or custom URL\n";
  print "\t\t  Valid names are:\n";
  print "\t\t    ", join( ", ", map { ( $_ =~ m/$opt_d/ ) ? "$_ (default)" : "$_" } sort keys %urls ), "\n";
  print "\t-f\tforce rebuild even if certdata.txt is current\n";
  print "\t-i\tprint version info about used modules\n";
  print "\t-k\tallow URLs other than HTTPS, enable HTTP fallback (insecure)\n";
  print "\t-l\tprint license info about certdata.txt\n";
  print "\t-m\tinclude meta data in output\n";
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

# Parses $param_string as a case insensitive comma separated list with optional
# whitespace validates that only allowed parameters are supplied
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
    # Tell the user which parameters were invalid and print the standard help
    # message which will exit
    print "Error: Invalid ", $description, scalar(@invalid) == 1 ? ": " : "s: ", join( ", ", map { "\"$_\"" } @invalid ), "\n";
    HELP_MESSAGE();
  }

  @values = @valid_values if ( is_in_list("ALL",@values) );

  return @values;
}

sub sha256 {
  my $result;
  if ($Digest::SHA::VERSION || $Digest::SHA::PurePerl::VERSION) {
    open(FILE, $_[0]) or die "Can't open '$_[0]': $!";
    binmode(FILE);
    $result = $MOD_SHA->new(256)->addfile(*FILE)->hexdigest;
    close(FILE);
  } else {
    # Use OpenSSL command if Perl Digest::SHA modules not available
    $result = `"$openssl" dgst -r -sha256 "$_[0]"`;
    $result =~ s/^([0-9a-f]{64}) .+/$1/is;
  }
  return $result;
}


sub oldhash {
  my $hash = "";
  open(C, "<$_[0]") || return 0;
  while(<C>) {
    chomp;
    if($_ =~ /^\#\# SHA256: (.*)/) {
      $hash = $1;
      last;
    }
  }
  close(C);
  return $hash;
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
    # for each level we want to output, see if any of our desired purposes are
    # included
    return 1 if ( defined( List::Util::first { is_in_list( $_, @included_mozilla_trust_purposes ) } @{$trust_purposes_by_level{$level}} ) );
  }

  return 0;
}

my $crt = $ARGV[0] || 'ca-bundle.crt';
(my $txt = $url) =~ s@(.*/|\?.*)@@g;

my $stdout = $crt eq '-';
my $resp;
my $fetched;

my $oldhash = oldhash($crt);

report "SHA256 of old file: $oldhash";

if(!$opt_n) {
  report "Downloading $txt ...";

  # If we have an HTTPS URL then use curl
  if($url =~ /^https:\/\//i) {
    my $curl = `curl -V`;
    if($curl) {
      if($curl =~ /^Protocols:.* https( |$)/m) {
        report "Get certdata with curl!";
        my $proto = !$opt_k ? "--proto =https" : "";
        my $quiet = $opt_q ? "-s" : "";
        my @out = `curl -w %{response_code} $proto $quiet -o "$txt" "$url"`;
        if(!$? && @out && $out[0] == 200) {
          $fetched = 1;
          report "Downloaded $txt";
        }
        else {
          report "Failed downloading via HTTPS with curl";
          if(-e $txt && !unlink($txt)) {
            report "Failed to remove '$txt': $!";
          }
        }
      }
      else {
        report "curl lacks https support";
      }
    }
    else {
      report "curl not found";
    }
  }

  # If nothing was fetched then use LWP
  if(!$fetched) {
    if($url =~ /^https:\/\//i) {
      report "Falling back to HTTP";
      $url =~ s/^https:\/\//http:\/\//i;
    }
    if(!$opt_k) {
      report "URLs other than HTTPS are disabled by default, to enable use -k";
      exit 1;
    }
    report "Get certdata with LWP!";
    if(!defined(${LWP::UserAgent::VERSION})) {
      report "LWP is not available (LWP::UserAgent not found)";
      exit 1;
    }
    my $ua  = new LWP::UserAgent(agent => "$0/$version");
    $ua->env_proxy();
    $resp = $ua->mirror($url, $txt);
    if($resp && $resp->code eq '304') {
      report "Not modified";
      exit 0 if -e $crt && !$opt_f;
    }
    else {
      $fetched = 1;
      report "Downloaded $txt";
    }
    if(!$resp || $resp->code !~ /^(?:200|304)$/) {
      report "Unable to download latest data: "
        . ($resp? $resp->code . ' - ' . $resp->message : "LWP failed");
      exit 1 if -e $crt || ! -r $txt;
    }
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
my $newhash= sha256($txt);

if(!$opt_f && $oldhash eq $newhash) {
    report "Downloaded file identical to previous run\'s source file. Exiting";
    if($opt_u && -e $txt && !unlink($txt)) {
        report "Failed to remove $txt: $!\n";
    }
    exit;
}

report "SHA256 of new file: $newhash";

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
## Certificate data from Mozilla ${datesrc}: ${currentdate} GMT
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
## SHA256: $newhash
##

EOT

report "Processing  '$txt' ...";
my $caname;
my $certnum = 0;
my $skipnum = 0;
my $start_of_cert = 0;
my $main_block = 0;
my $main_block_name;
my $trust_block = 0;
my $trust_block_name;
my @precert;
my $cka_value;
my $valid = 0;

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
    next;
  }
  # The input file format consists of blocks of Mozilla objects.
  # The blocks are separated by blank lines but may be related.
  elsif(/^\s*$/) {
    $main_block = 0;
    $trust_block = 0;
    next;
  }
  # Each certificate has a main block.
  elsif(/^# Certificate "(.*)"/) {
    (!$main_block && !$trust_block) or die "Unexpected certificate block";
    $main_block = 1;
    $main_block_name = $1;
    # Reset all other certificate variables.
    $trust_block = 0;
    $trust_block_name = "";
    $valid = 0;
    $start_of_cert = 0;
    $caname = "";
    $cka_value = "";
    undef @precert;
    next;
  }
  # Each certificate's main block is followed by a trust block.
  elsif(/^# Trust for (?:Certificate )?"(.*)"/) {
    (!$main_block && !$trust_block) or die "Unexpected trust block";
    $trust_block = 1;
    $trust_block_name = $1;
    if($main_block_name ne $trust_block_name) {
      die "cert name \"$main_block_name\" != trust name \"$trust_block_name\"";
    }
    next;
  }
  # Ignore other blocks.
  #
  # There is a documentation comment block, a BEGINDATA block, and a bunch of
  # blocks starting with "# Explicitly Distrust <certname>".
  #
  # The latter is for certificates that have already been removed and are not
  # included. Not all explicitly distrusted certificates are ignored at this
  # point, just those without an actual certificate.
  elsif(!$main_block && !$trust_block) {
    next;
  }
  elsif(/^#/) {
    # The commented lines in a main block are plaintext metadata that describes
    # the certificate. Issuer, Subject, Fingerprint, etc.
    if($main_block) {
      push @precert, $_ if not /^#$/;
      if(/^# Not Valid After : (.*)/) {
        my $stamp = $1;
        use Time::Piece;
        # Not Valid After : Thu Sep 30 14:01:15 2021
        my $t = Time::Piece->strptime($stamp, "%a %b %d %H:%M:%S %Y");
        my $delta = ($t->epoch - time()); # negative means no longer valid
        if($delta < 0) {
          $skipnum++;
          report "Skipping: $main_block_name is not valid anymore" if ($opt_v);
          $valid = 0;
        }
        else {
          $valid = 1;
        }
      }
    }
    next;
  }
  elsif(!$valid) {
    next;
  }

  chomp;

  if($main_block) {
    if(/^CKA_CLASS CK_OBJECT_CLASS CKO_CERTIFICATE/) {
      !$start_of_cert or die "Duplicate CKO_CERTIFICATE object";
      $start_of_cert = 1;
      next;
    }
    elsif(!$start_of_cert) {
      next;
    }
    elsif(/^CKA_LABEL UTF8 \"(.*)\"/) {
      ($caname eq "") or die "Duplicate CKA_LABEL attribute";
      $caname = $1;
      if($caname ne $main_block_name) {
        die "caname \"$caname\" != cert name \"$main_block_name\"";
      }
      next;
    }
    elsif(/^CKA_VALUE MULTILINE_OCTAL/) {
      ($cka_value eq "") or die "Duplicate CKA_VALUE attribute";
      while (<TXT>) {
        last if (/^END/);
        chomp;
        my @octets = split(/\\/);
        shift @octets;
        for (@octets) {
          $cka_value .= chr(oct);
        }
      }
      next;
    }
    elsif (/^CKA_NSS_SERVER_DISTRUST_AFTER (CK_BBOOL CK_FALSE|MULTILINE_OCTAL)/) {
      # Example:
      # CKA_NSS_SERVER_DISTRUST_AFTER MULTILINE_OCTAL
      # \062\060\060\066\061\067\060\060\060\060\060\060\132
      # END
      if($1 eq "MULTILINE_OCTAL") {
        my @timestamp;
        while (<TXT>) {
          last if (/^END/);
          chomp;
          my @octets = split(/\\/);
          shift @octets;
          for (@octets) {
            push @timestamp, chr(oct);
          }
        }
        scalar(@timestamp) == 13 or die "Failed parsing timestamp";
        # A trailing Z in the timestamp signifies UTC
        if($timestamp[12] ne "Z") {
          report "distrust date stamp is not using UTC";
        }
        # Example date: 200617000000Z
        # Means 2020-06-17 00:00:00 UTC
        my $distrustat =
          timegm($timestamp[10] . $timestamp[11], # second
                 $timestamp[8] . $timestamp[9],   # minute
                 $timestamp[6] . $timestamp[7],   # hour
                 $timestamp[4] . $timestamp[5],   # day
                 ($timestamp[2] . $timestamp[3]) - 1, # month
                 "20" . $timestamp[0] . $timestamp[1]); # year
        if(time >= $distrustat) {
          # not trusted anymore
          $skipnum++;
          report "Skipping: $main_block_name is not trusted anymore" if ($opt_v);
          $valid = 0;
        }
        else {
          # still trusted
        }
      }
      next;
    }
    else {
      next;
    }
  }

  if(!$trust_block || !$start_of_cert || $caname eq "" || $cka_value eq "") {
    die "Certificate extraction failed";
  }

  my %trust_purposes_by_level;

  if(/^CKA_CLASS CK_OBJECT_CLASS CKO_NSS_TRUST/) {
    # now scan the trust part to determine how we should trust this cert
    while (<TXT>) {
      if(/^\s*$/) {
        $trust_block = 0;
        last;
      }
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

    # Sanity check that an explicitly distrusted certificate only has trust
    # purposes with a trust level of NOT_TRUSTED.
    #
    # Certificate objects that are explicitly distrusted are in a certificate
    # block that starts # Certificate "Explicitly Distrust(ed) <certname>",
    # where "Explicitly Distrust(ed) " was prepended to the original cert name.
    if($caname =~ /distrust/i ||
       $main_block_name =~ /distrust/i ||
       $trust_block_name =~ /distrust/i) {
      my @levels = keys %trust_purposes_by_level;
      if(scalar(@levels) != 1 || $levels[0] ne "NOT_TRUSTED") {
        die "\"$caname\" must have all trust purposes at level NOT_TRUSTED.";
      }
    }

    if ( !should_output_cert(%trust_purposes_by_level) ) {
      $skipnum ++;
      report "Skipping: $caname lacks acceptable trust level" if ($opt_v);
    } else {
      my $encoded = MIME::Base64::encode_base64($cka_value, '');
      $encoded =~ s/(.{1,${opt_w}})/$1\n/g;
      my $pem = "-----BEGIN CERTIFICATE-----\n"
              . $encoded
              . "-----END CERTIFICATE-----\n";
      print CRT "\n$caname\n";
      my $maxStringLength = length(decode('UTF-8', $caname, Encode::FB_CROAK | Encode::LEAVE_SRC));
      print CRT ("=" x $maxStringLength . "\n");
      if ($opt_t) {
        foreach my $key (sort keys %trust_purposes_by_level) {
           my $string = $key . ": " . join(", ", @{$trust_purposes_by_level{$key}});
           print CRT $string . "\n";
        }
      }
      if($opt_m) {
        print CRT for @precert;
      }
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
      report "Processed: $caname" if ($opt_v);
      $certnum ++;
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
if($opt_u && -e $txt && !unlink($txt)) {
  report "Failed to remove $txt: $!\n";
}
report "Done ($certnum CA certs processed, $skipnum skipped).";
