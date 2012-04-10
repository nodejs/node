=pod

=head1 NAME

SSL_CTX_load_verify_locations - set default locations for trusted CA
certificates

=head1 SYNOPSIS

 #include <openssl/ssl.h>

 int SSL_CTX_load_verify_locations(SSL_CTX *ctx, const char *CAfile,
                                   const char *CApath);

=head1 DESCRIPTION

SSL_CTX_load_verify_locations() specifies the locations for B<ctx>, at
which CA certificates for verification purposes are located. The certificates
available via B<CAfile> and B<CApath> are trusted.

=head1 NOTES

If B<CAfile> is not NULL, it points to a file of CA certificates in PEM
format. The file can contain several CA certificates identified by

 -----BEGIN CERTIFICATE-----
 ... (CA certificate in base64 encoding) ...
 -----END CERTIFICATE-----

sequences. Before, between, and after the certificates text is allowed
which can be used e.g. for descriptions of the certificates.

The B<CAfile> is processed on execution of the SSL_CTX_load_verify_locations()
function.

If B<CApath> is not NULL, it points to a directory containing CA certificates
in PEM format. The files each contain one CA certificate. The files are
looked up by the CA subject name hash value, which must hence be available.
If more than one CA certificate with the same name hash value exist, the
extension must be different (e.g. 9d66eef0.0, 9d66eef0.1 etc). The search
is performed in the ordering of the extension number, regardless of other
properties of the certificates.
Use the B<c_rehash> utility to create the necessary links.

The certificates in B<CApath> are only looked up when required, e.g. when
building the certificate chain or when actually performing the verification
of a peer certificate.

When looking up CA certificates, the OpenSSL library will first search the
certificates in B<CAfile>, then those in B<CApath>. Certificate matching
is done based on the subject name, the key identifier (if present), and the
serial number as taken from the certificate to be verified. If these data
do not match, the next certificate will be tried. If a first certificate
matching the parameters is found, the verification process will be performed;
no other certificates for the same parameters will be searched in case of
failure.

In server mode, when requesting a client certificate, the server must send
the list of CAs of which it will accept client certificates. This list
is not influenced by the contents of B<CAfile> or B<CApath> and must
explicitly be set using the
L<SSL_CTX_set_client_CA_list(3)|SSL_CTX_set_client_CA_list(3)>
family of functions.

When building its own certificate chain, an OpenSSL client/server will
try to fill in missing certificates from B<CAfile>/B<CApath>, if the
certificate chain was not explicitly specified (see
L<SSL_CTX_add_extra_chain_cert(3)|SSL_CTX_add_extra_chain_cert(3)>,
L<SSL_CTX_use_certificate(3)|SSL_CTX_use_certificate(3)>.

=head1 WARNINGS

If several CA certificates matching the name, key identifier, and serial
number condition are available, only the first one will be examined. This
may lead to unexpected results if the same CA certificate is available
with different expiration dates. If a "certificate expired" verification
error occurs, no other certificate will be searched. Make sure to not
have expired certificates mixed with valid ones.

=head1 EXAMPLES

Generate a CA certificate file with descriptive text from the CA certificates
ca1.pem ca2.pem ca3.pem:

 #!/bin/sh
 rm CAfile.pem
 for i in ca1.pem ca2.pem ca3.pem ; do
   openssl x509 -in $i -text >> CAfile.pem
 done

Prepare the directory /some/where/certs containing several CA certificates
for use as B<CApath>:

 cd /some/where/certs
 c_rehash .

=head1 RETURN VALUES

The following return values can occur:

=over 4

=item 0

The operation failed because B<CAfile> and B<CApath> are NULL or the
processing at one of the locations specified failed. Check the error
stack to find out the reason.

=item 1

The operation succeeded.

=back

=head1 SEE ALSO

L<ssl(3)|ssl(3)>,
L<SSL_CTX_set_client_CA_list(3)|SSL_CTX_set_client_CA_list(3)>,
L<SSL_get_client_CA_list(3)|SSL_get_client_CA_list(3)>,
L<SSL_CTX_use_certificate(3)|SSL_CTX_use_certificate(3)>,
L<SSL_CTX_add_extra_chain_cert(3)|SSL_CTX_add_extra_chain_cert(3)>,
L<SSL_CTX_set_cert_store(3)|SSL_CTX_set_cert_store(3)>

=cut
