=pod

=head1 NAME

SSL_CTX_add_extra_chain_cert - add certificate to chain

=head1 SYNOPSIS

 #include <openssl/ssl.h>

 long SSL_CTX_add_extra_chain_cert(SSL_CTX ctx, X509 *x509)

=head1 DESCRIPTION

SSL_CTX_add_extra_chain_cert() adds the certificate B<x509> to the certificate
chain presented together with the certificate. Several certificates
can be added one after the other.

=head1 NOTES

When constructing the certificate chain, the chain will be formed from
these certificates explicitly specified. If no chain is specified,
the library will try to complete the chain from the available CA
certificates in the trusted CA storage, see
L<SSL_CTX_load_verify_locations(3)|SSL_CTX_load_verify_locations(3)>.

=head1 RETURN VALUES

SSL_CTX_add_extra_chain_cert() returns 1 on success. Check out the
error stack to find out the reason for failure otherwise.

=head1 SEE ALSO

L<ssl(3)|ssl(3)>,
L<SSL_CTX_use_certificate(3)|SSL_CTX_use_certificate(3)>,
L<SSL_CTX_set_client_cert_cb(3)|SSL_CTX_set_client_cert_cb(3)>,
L<SSL_CTX_load_verify_locations(3)|SSL_CTX_load_verify_locations(3)>

=cut
