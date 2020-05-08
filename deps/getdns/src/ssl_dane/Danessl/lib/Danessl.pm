package Danessl;

use 5.012005;
use strict;
use warnings;
use Carp;

require Exporter;
use AutoLoader;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Danessl ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	USAGE_PKIX_TA
	USAGE_PKIX_EE
	USAGE_DANE_TA
	USAGE_DANE_EE
	SELECTOR_CERT
	SELECTOR_SPKI
	MATCHING_FULL
	MATCHING_2256
	MATCHING_2512
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	USAGE_PKIX_TA
	USAGE_PKIX_EE
	USAGE_DANE_TA
	USAGE_DANE_EE
	SELECTOR_CERT
	SELECTOR_SPKI
	MATCHING_FULL
	MATCHING_2256
	MATCHING_2512
);

our $VERSION = '0.01';

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.

    my $constname;
    our $AUTOLOAD;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "&Danessl::constant not defined" if $constname eq 'constant';
    my ($error, $val) = constant($constname);
    if ($error) { croak $error; }
    {
	no strict 'refs';
	# Fixed between 5.005_53 and 5.005_61
#XXX	if ($] >= 5.00561) {
#XXX	    *$AUTOLOAD = sub () { $val };
#XXX	}
#XXX	else {
	    *$AUTOLOAD = sub { $val };
#XXX	}
    }
    goto &$AUTOLOAD;
}

require XSLoader;
XSLoader::load('Danessl', $VERSION);

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=head1 NAME

Danessl - Validate SSL certificate chains via DANE TLSA records

=head1 SYNOPSIS

  use Danessl qw(USAGE_PKIX_TA USAGE_PKIX_EE
		 USAGE_DANE_TA USAGE_DANE_EE
	         SELECTOR_CERT SELECTOR_SPKI
	         MATCHING_2256 MATCHING_2512);

  # Verify record syntax, throws exception on error
  #
  eval { Danessl::verify($usage, $selector, $mtype, $data) };

  # Verify a PEM-encoded certificate $chain, returning
  # match depth and matching peer name when applicable.
  # Throws exception when no match is found.
  #
  my ($depth, $hostname) = eval {
      Danessl::verify(@tlsa, $chain, $tlsa_base_domain,
		      @additional_names);
  };

  # Generate TLSA record matching certificate at depth
  # $depth in PEM encoded $chain with TLSA parameters
  # $usage, $selector and $mtype.
  #
  my ($depth, $mhost, $data) =
    eval { Danessl::tlsagen($chain, $depth, $tlsa_base_domain,
			    $usage, $selector, $mtype) };

=head1 DESCRIPTION

The Danessl module makes it possible to check the validity of TLSA
records, use TLSA records to verify certificate chains and to
generate TLSA records that match a given certificate chain.

=head2 Exportable constants

  USAGE_PKIX_TA
  USAGE_PKIX_EE
  USAGE_DANE_TA
  USAGE_DANE_EE
  SELECTOR_CERT
  SELECTOR_SPKI
  MATCHING_FULL
  MATCHING_2256
  MATCHING_2512

=head1 METHODS

=over 4

=item verify(usage, selector, mtype, data)

When called with just four arguments C<verify> validates the syntax
of a TLSA record.  It checks for supported values of the C<usage>,
C<selector> and C<mtype>.  The C<data> argument must be a hexadecimal
string.  If C<mtype> is non-zero (a digest algorithm id), the data
length must be consistent wit the digest algorithm.  If C<mtype>
is 0 (indicating verbatim DER data), the data must be the hexadecimal
encoding a certificate or public key in agreement with the C<selector>.
An exception is thrown if a problems is found.

=item verify(usage, selector, mtype, data, chain, [base, [names, ...]])

When called with 5 or more  arguments C<verify> checks whether a
certificate C<chain> (passed a single PEM-encoded string with the
leaf certificate first) matches the given TLSA record.  If C<usage>
is not DANE-EE(3) the TLSA C<base> domain is required, and is used
to perform peer name checks.  Additional acceptable C<names> for
the peer can be provided in arguments that follow the C<base> domain.

When verification succeeds, the depth of the matching certificate
in the validated chain (possibly re-ordered if C<chain> is not
ordered correctly from leaf to trust-anchor) is returned with the
matching name from the peer certificate if C<usage> is not equal
to 3 (with DANE-EE(3) name checks don't apply).  An exception
is thrown if verification fails.

=item tlsagen(chain, offset, base, usage, selector, mtype)

Generates a TLSA record with parameters C<usage>, C<selector> and
C<mtype> matching the certificate at position C<offset> in the
PEM-encoded C<chain> (the leaf certificate offset is 0).  If C<usage>
is not 3, the leaf certificate MUST pass name checks with C<base>
as the reference identifier or else an exception is thrown.

On success a triple (C<depth>, C<mhost>, C<data>) is returned in
which C<depth> is the depth of the selected certificate in a correctly
ordered verified chain, C<mhost> is the matching name from the peer
certificate (undefined when C<usage> is 3) and C<data> is the
hexadecimal encoding of the associated data element of the desired
TLSA record.

An exception is thrown if the leaf certificate fails name checks,
or the input arguments are invalid.

=back

=head1 SEE ALSO

L<Net::DNS>, L<Net::SSLeay>.

You can find the latest source code for this module at:
L<https://github.com/vdukhovni/ssl_dane>.

=head1 AUTHOR

Viktor Dukhovni, E<lt>postfix-users@dukhovni.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2014 by Viktor Dukhovni

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.12.5 or,
at your option, any later version of Perl 5 you may have available.

=cut
