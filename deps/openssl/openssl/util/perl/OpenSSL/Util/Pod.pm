# Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

package OpenSSL::Util::Pod;

use strict;
use warnings;

use Exporter;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
$VERSION = "0.1";
@ISA = qw(Exporter);
@EXPORT = qw(extract_pod_info);
@EXPORT_OK = qw();

=head1 NAME

OpenSSL::Util::Pod - utilities to manipulate .pod files

=head1 SYNOPSIS

  use OpenSSL::Util::Pod;

  my %podinfo = extract_pod_info("foo.pod");

  # or if the file is already opened...  Note that this consumes the
  # remainder of the file.

  my %podinfo = extract_pod_info(\*STDIN);

=head1 DESCRIPTION

=over

=item B<extract_pod_info "FILENAME", HASHREF>

=item B<extract_pod_info "FILENAME">

=item B<extract_pod_info GLOB, HASHREF>

=item B<extract_pod_info GLOB>

Extracts information from a .pod file, given a STRING (file name) or a
GLOB (a file handle).  The result is given back as a hash table.

The additional hash is for extra parameters:

=over

=item B<section =E<gt> N>

The value MUST be a number, and will be the default man section number
to be used with the given .pod file.  This number can be altered if
the .pod file has a line like this:

  =for comment openssl_manual_section: 4

=item B<debug =E<gt> 0|1>

If set to 1, extra debug text will be printed on STDERR

=back

=back

=head1 RETURN VALUES

=over

=item B<extract_pod_info> returns a hash table with the following
items:

=over

=item B<section =E<gt> N>

The man section number this .pod file belongs to.  Often the same as
was given as input.

=item B<names =E<gt> [ "name", ... ]>

All the names extracted from the NAME section.

=back

=back

=cut

sub extract_pod_info {
    my $input = shift;
    my $defaults_ref = shift || {};
    my %defaults = ( debug => 0, section => 0, %$defaults_ref );
    my $fh = undef;
    my $filename = undef;

    # If not a file handle, then it's assume to be a file path (a string)
    unless (ref $input eq "GLOB") {
        $filename = $input;
        open $fh, $input or die "Trying to read $filename: $!\n";
        print STDERR "DEBUG: Reading $input\n" if $defaults{debug};
        $input = $fh;
    }

    my %podinfo = ( section => $defaults{section});
    while(<$input>) {
        s|\R$||;
        if (m|^=for\s+comment\s+openssl_manual_section:\s*([0-9])\s*$|) {
            print STDERR "DEBUG: Found man section number $1\n"
                if $defaults{debug};
            $podinfo{section} = $1;
        }

        # Stop reading when we have reached past the NAME section.
        last if (m|^=head1|
                 && defined $podinfo{lastsect}
                 && $podinfo{lastsect} eq "NAME");

        # Collect the section name
        if (m|^=head1\s*(.*)|) {
            $podinfo{lastsect} = $1;
            $podinfo{lastsect} =~ s/\s+$//;
            print STDERR "DEBUG: Found new pod section $1\n"
                if $defaults{debug};
            print STDERR "DEBUG: Clearing pod section text\n"
                if $defaults{debug};
            $podinfo{lastsecttext} = "";
        }

        next if (m|^=| || m|^\s*$|);

        # Collect the section text
        print STDERR "DEBUG: accumulating pod section text \"$_\"\n"
            if $defaults{debug};
        $podinfo{lastsecttext} .= " " if $podinfo{lastsecttext};
        $podinfo{lastsecttext} .= $_;
    }


    if (defined $fh) {
        close $fh;
        print STDERR "DEBUG: Done reading $filename\n" if $defaults{debug};
    }

    $podinfo{lastsecttext} =~ s| - .*$||;

    my @names =
        map { s|\s+||g; $_ }
        split(m|,|, $podinfo{lastsecttext});

    return ( section => $podinfo{section}, names => [ @names ] );
}

1;
