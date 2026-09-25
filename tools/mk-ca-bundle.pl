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
use File::Basename 'dirname';
use Getopt::Std;
use MIME::Base64;
use strict;
use warnings;
use vars qw($opt_h $opt_i $opt_l $opt_m $opt_p $opt_q $opt_s $opt_t $opt_v $opt_w);
use List::Util;
use Text::Wrap;

# If the OpenSSL commandline is not in search path you can configure it here!
my $openssl = 'openssl';

my $version = '1.33';

$opt_w = 72; # default base64 encoded lines length

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
    "NOT_TRUSTED",          # Do not trust these certs.
    "MUST_VERIFY_TRUST",    # This explicitly tells us that it IS NOT a CA but is
                            # otherwise ok. In other words, this should tell the
                            # app to ignore any other sources that claim this is
                            # a CA.
    "TRUSTED"               # This cert is trusted, but only for itself and not
                            # for delegates (i.e. it is not a CA).
);

my $default_signature_algorithms = $opt_s = "SHA256";

my @valid_signature_algorithms = (
    "SHA256",
    "SHA384",
    "SHA512"
);

$0 =~ s@.*(/|\\)@@;
$Getopt::Std::STANDARD_HELP_VERSION = 1;
getopts('hilmp:qs:tvw:');

if($opt_i) {
    print ("=" x 78 . "\n");
    print "Script Version                   : $version\n";
    print "Perl Version                     : $]\n";
    print "Operating System Name            : $^O\n";
    print "Getopt::Std.pm Version           : ${Getopt::Std::VERSION}\n";
    print "MIME::Base64.pm Version          : ${MIME::Base64::VERSION}\n";
    print ("=" x 78 . "\n");
}

sub HELP_MESSAGE() {
    print "Usage:\t${0} [-i] [-l] [-m] [-p<purposes:levels>] [-q] [-s<algorithms>] [-t] [-v] [-w<l>] [<outputfile>]\n";
    print "\t-i\tprint version info about used modules\n";
    print "\t-l\tprint license info about certdata.txt\n";
    print "\t-m\tinclude meta data in output\n";
    print wrap("\t","\t\t", "-p\tlist of Mozilla trust purposes and levels for certificates to include in output. " .
          "Takes the form of a comma separated list of purposes, a colon, and a comma separated list of levels. " .
          "(default: $default_mozilla_trust_purposes:$default_mozilla_trust_levels)"), "\n";
    print "\t\t  Valid purposes are:\n";
    print wrap("\t\t    ","\t\t    ", join(", ", "ALL", @valid_mozilla_trust_purposes)), "\n";
    print "\t\t  Valid levels are:\n";
    print wrap("\t\t    ","\t\t    ", join(", ", "ALL", @valid_mozilla_trust_levels)), "\n";
    print "\t-q\tbe really quiet (no progress output at all)\n";
    print wrap("\t","\t\t", "-s\tcomma separated list of certificate signatures/hashes to output in plain text mode. (default: $default_signature_algorithms)\n");
    print "\t\t  Valid signature algorithms are:\n";
    print wrap("\t\t    ","\t\t    ", join(", ", "ALL", @valid_signature_algorithms)), "\n";
    print "\t-t\tinclude plain text listing of certificates\n";
    print "\t-v\tbe verbose and print out processed CAs\n";
    print "\t-w <l>\twrap base64 output lines after <l> chars (default: ${opt_w})\n";
    exit;
}

sub VERSION_MESSAGE() {
    print "${0} version ${version} running Perl ${]} on ${^O}\n";
}

HELP_MESSAGE() if($opt_h);

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
    } split(',', $param_string);

    # Find all values which are not in the list of valid values or "ALL"
    my @invalid = grep { !is_in_list($_, "ALL", @valid_values) } @values;

    if(scalar(@invalid) > 0) {
        # Tell the user which parameters were invalid and print the standard help
        # message which also exits
        print "Error: Invalid ", $description, scalar(@invalid) == 1 ? ": " : "s: ", join(", ", map { "\"$_\"" } @invalid), "\n";
        HELP_MESSAGE();
    }

    @values = @valid_values if(is_in_list("ALL", @values));

    return @values;
}

if($opt_p !~ m/:/) {
    print "Error: Mozilla trust identifier list must include both purposes and levels\n";
    HELP_MESSAGE();
}

(my $included_mozilla_trust_purposes_string, my $included_mozilla_trust_levels_string) = split(':', $opt_p);
my @included_mozilla_trust_purposes = parse_csv_param("trust purpose", $included_mozilla_trust_purposes_string, @valid_mozilla_trust_purposes);
my @included_mozilla_trust_levels = parse_csv_param("trust level", $included_mozilla_trust_levels_string, @valid_mozilla_trust_levels);

my @included_signature_algorithms = parse_csv_param("signature algorithm", $opt_s, @valid_signature_algorithms);

sub should_output_cert(%) {
    my %trust_purposes_by_level = @_;

    foreach my $level (@included_mozilla_trust_levels) {
        # for each level we want to output, see if any of our desired purposes are
        # included
        return 1 if(defined(List::Util::first { is_in_list($_, @included_mozilla_trust_purposes) } @{$trust_purposes_by_level{$level}}));
    }

    return 0;
}

my $crt = $ARGV[0] || dirname(__FILE__) . '/../src/node_root_certs.h';
my $txt = dirname(__FILE__) . '/certdata.txt';

my $stdout = $crt eq '-';

if($stdout) {
    open(CRT, '> -') or die "Could not open STDOUT: $!\n";
} else {
    open(CRT, ">", "$crt.~") or die "Could not open $crt.~: $!\n";
}

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

open(TXT, $txt) or die "Could not open $txt: $!\n";
print CRT "#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS\n";
while(<TXT>) {
    if(/\*\*\*\*\* BEGIN LICENSE BLOCK \*\*\*\*\*/) {
        print CRT;
        print if($opt_l);
        while(<TXT>) {
            print CRT;
            print if($opt_l);
            last if(/\*\*\*\*\* END LICENSE BLOCK \*\*\*\*\*/);
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
    # point, only those without an actual certificate.
    elsif(!$main_block && !$trust_block) {
        next;
    }
    elsif(/^#/) {
        # The commented lines in a main block are plaintext metadata that describes
        # the certificate. Issuer, Subject, Fingerprint, etc.
        if($main_block) {
            push @precert, s{^#}{//}r if not /^#$/;
            if(/^# Not Valid After : (.*)/) {
                my $stamp = $1;
                use Time::Piece;
                # Not Valid After : Thu Sep 30 14:01:15 2021
                my $t = Time::Piece->strptime($stamp, "%a %b %d %H:%M:%S %Y");
                my $delta = ($t->epoch - time()); # negative means no longer valid
                if($delta < 0) {
                    $skipnum++;
                    report "Skipping: $main_block_name is not valid anymore" if($opt_v);
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
            while(<TXT>) {
                last if(/^END/);
                chomp;
                my @octets = split(/\\/);
                shift @octets;
                for(@octets) {
                    $cka_value .= chr(oct);
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
        while(<TXT>) {
            if(/^\s*$/) {
                $trust_block = 0;
                last;
            }
            if(/^CKA_TRUST_([A-Z_]+)\s+CK_TRUST\s+CKT_NSS_([A-Z_]+)\s*$/) {
                if(!is_in_list($1, @valid_mozilla_trust_purposes)) {
                    report "Warning: Unrecognized trust purpose for cert: $caname. Trust purpose: $1. Trust Level: $2";
                } elsif(!is_in_list($2, @valid_mozilla_trust_levels)) {
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

        if(!should_output_cert(%trust_purposes_by_level)) {
            $skipnum ++;
            report "Skipping: $caname lacks acceptable trust level" if($opt_v);
        } else {
            my $encoded = MIME::Base64::encode_base64($cka_value, '');
            $encoded =~ s/(.{1,${opt_w}})/"$1\\n"\n/g;
            my $pem = "\"-----BEGIN CERTIFICATE-----\\n\"\n"
                    . $encoded
                    . "\"-----END CERTIFICATE-----\",\n";
            print CRT "\n/* $caname */\n";
            if($opt_t) {
                foreach my $key (sort keys %trust_purposes_by_level) {
                    my $string = $key . ": " . join(", ", @{$trust_purposes_by_level{$key}});
                    print CRT $string . "\n";
                }
            }
            if($opt_m) {
                print CRT for @precert;
            }
            if(!$opt_t) {
                print CRT $pem;
            } else {
                my $pipe = "";
                foreach my $hash (@included_signature_algorithms) {
                    $pipe = "|$openssl x509 -" . $hash . " -fingerprint -noout -inform PEM";
                    if(!$stdout) {
                        $pipe .= " >> $crt.~";
                        close(CRT) or die "Could not close $crt.~: $!";
                    }
                    open(TMP, $pipe) or die "Could not open openssl pipe: $!";
                    print TMP $pem;
                    close(TMP) or die "Could not close openssl pipe: $!";
                    if(!$stdout) {
                        open(CRT, ">>", "$crt.~") or die "Could not open $crt.~: $!";
                    }
                }
                $pipe = "|$openssl x509 -text -inform PEM";
                if(!$stdout) {
                    $pipe .= " >> $crt.~";
                    close(CRT) or die "Could not close $crt.~: $!";
                }
                open(TMP, $pipe) or die "Could not open openssl pipe: $!";
                print TMP $pem;
                close(TMP) or die "Could not close openssl pipe: $!";
                if(!$stdout) {
                    open(CRT, ">>", "$crt.~") or die "Could not open $crt.~: $!";
                }
            }
            report "Processed: $caname" if($opt_v);
            $certnum++;
        }
    }
}
print CRT "#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS\n";
close(TXT) or die "Could not close $txt: $!\n";
close(CRT) or die "Could not close $crt.~: $!\n";
unless($stdout) {
    rename "$crt.~", $crt or die "Failed to rename $crt.~ to $crt: $!\n";
}
report "Done ($certnum CA certs processed, $skipnum skipped).";
