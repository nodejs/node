=pod

=head1 NAME

X509_verify_cert - discover and verify X509 certificte chain

=head1 SYNOPSIS

 #include <openssl/x509.h>

 int X509_verify_cert(X509_STORE_CTX *ctx);

=head1 DESCRIPTION

The X509_verify_cert() function attempts to discover and validate a
certificate chain based on parameters in B<ctx>. A complete description of
the process is contained in the L<verify(1)|verify(1)> manual page.

=head1 RETURN VALUES

If a complete chain can be built and validated this function returns 1,
otherwise it return zero, in exceptional circumstances it can also
return a negative code.

If the function fails additional error information can be obtained by
examining B<ctx> using, for example X509_STORE_CTX_get_error().

=head1 NOTES

Applications rarely call this function directly but it is used by
OpenSSL internally for certificate validation, in both the S/MIME and
SSL/TLS code.

The negative return value from X509_verify_cert() can only occur if no
certificate is set in B<ctx> (due to a programming error); if X509_verify_cert()
twice without reinitialising B<ctx> in between; or if a retry
operation is requested during internal lookups (which never happens with
standard lookup methods). It is however recommended that application check
for <= 0 return value on error.

=head1 BUGS

This function uses the header B<x509.h> as opposed to most chain verification
functiosn which use B<x509_vfy.h>.

=head1 SEE ALSO

L<X509_STORE_CTX_get_error(3)|X509_STORE_CTX_get_error(3)>

=head1 HISTORY

X509_verify_cert() is available in all versions of SSLeay and OpenSSL.

=cut
