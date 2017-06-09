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
use File::Basename 'dirname';
use Getopt::Std;
use MIME::Base64;
use strict;
use vars qw($opt_h $opt_i $opt_l $opt_p $opt_q $opt_s $opt_t $opt_v $opt_w);
use List::Util;
use Text::Wrap;

# If the OpenSSL commandline is not in search path you can configure it here!
my $openssl = 'openssl';

my $version = '1.25';

$opt_w = 72; # default base64 encoded lines length

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

if ($opt_i) {
  print ("=" x 78 . "\n");
  print "Script Version                   : $version\n";
  print "Perl Version                     : $]\n";
  print "Operating System Name            : $^O\n";
  print "Getopt::Std.pm Version           : ${Getopt::Std::VERSION}\n";
  print "MIME::Base64.pm Version          : ${MIME::Base64::VERSION}\n";
  print ("=" x 78 . "\n");
}

sub HELP_MESSAGE() {
  print "Usage:\t${0} [-i] [-l] [-p<purposes:levels>] [-q] [-s<algorithms>] [-t] [-v] [-w<l>] [<outputfile>]\n";
  print "\t-i\tprint version info about used modules\n";
  print "\t-l\tprint license info about certdata.txt\n";
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
  print "\t-v\tbe verbose and print out processed CAs\n";
  print "\t-w <l>\twrap base64 output lines after <l> chars (default: ${opt_w})\n";
  exit;
}

sub VERSION_MESSAGE() {
  print "${0} version ${version} running Perl ${]} on ${^O}\n";
}

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

my $crt = $ARGV[0] || dirname(__FILE__) . '/../src/node_root_certs.h';
my $txt = dirname(__FILE__) . '/certdata.txt';

my $stdout = $crt eq '-';

if( $stdout ) {
    open(CRT, '> -') or die "Couldn't open STDOUT: $!\n";
} else {
    open(CRT,">$crt.~") or die "Couldn't open $crt.~: $!\n";
}

my $caname;
my $certnum = 0;
my $skipnum = 0;
my $start_of_cert = 0;

open(TXT,"$txt") or die "Couldn't open $txt: $!\n";
print CRT "#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS\n";
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
    print CRT "/* $1 */\n";
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
      $encoded =~ s/(.{1,${opt_w}})/"$1\\n"\n/g;
      my $pem = "\"-----BEGIN CERTIFICATE-----\\n\"\n"
              . $encoded
              . "\"-----END CERTIFICATE-----\\n\",\n";
      print CRT "\n/* $caname */\n";

      my $maxStringLength = length($caname);
      if ($opt_t) {
        foreach my $key (keys %trust_purposes_by_level) {
           my $string = $key . ": " . join(", ", @{$trust_purposes_by_level{$key}});
           $maxStringLength = List::Util::max( length($string), $maxStringLength );
           print CRT $string . "\n";
        }
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
      report "Parsing: $caname" if ($opt_v);
      $certnum ++;
      $start_of_cert = 0;
    }
  }
}
print CRT "#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS\n";
close(TXT) or die "Couldn't close $txt: $!\n";
close(CRT) or die "Couldn't close $crt.~: $!\n";
unless( $stdout ) {
    rename "$crt.~", $crt or die "Failed to rename $crt.~ to $crt: $!\n";
}
report "Done ($certnum CA certs processed, $skipnum skipped).";
