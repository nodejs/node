=pod

=head1 NAME

SSL_CTX_set_tmp_rsa_callback, SSL_CTX_set_tmp_rsa, SSL_CTX_need_tmp_rsa, SSL_set_tmp_rsa_callback, SSL_set_tmp_rsa, SSL_need_tmp_rsa - handle RSA keys for ephemeral key exchange

=head1 SYNOPSIS

 #include <openssl/ssl.h>

 void SSL_CTX_set_tmp_rsa_callback(SSL_CTX *ctx,
            RSA *(*tmp_rsa_callback)(SSL *ssl, int is_export, int keylength));
 long SSL_CTX_set_tmp_rsa(SSL_CTX *ctx, RSA *rsa);
 long SSL_CTX_need_tmp_rsa(SSL_CTX *ctx);

 void SSL_set_tmp_rsa_callback(SSL_CTX *ctx,
            RSA *(*tmp_rsa_callback)(SSL *ssl, int is_export, int keylength));
 long SSL_set_tmp_rsa(SSL *ssl, RSA *rsa)
 long SSL_need_tmp_rsa(SSL *ssl)

 RSA *(*tmp_rsa_callback)(SSL *ssl, int is_export, int keylength);

=head1 DESCRIPTION

SSL_CTX_set_tmp_rsa_callback() sets the callback function for B<ctx> to be
used when a temporary/ephemeral RSA key is required to B<tmp_rsa_callback>.
The callback is inherited by all SSL objects newly created from B<ctx>
with <SSL_new(3)|SSL_new(3)>. Already created SSL objects are not affected.

SSL_CTX_set_tmp_rsa() sets the temporary/ephemeral RSA key to be used to be
B<rsa>. The key is inherited by all SSL objects newly created from B<ctx>
with <SSL_new(3)|SSL_new(3)>. Already created SSL objects are not affected.

SSL_CTX_need_tmp_rsa() returns 1, if a temporary/ephemeral RSA key is needed
for RSA-based strength-limited 'exportable' ciphersuites because a RSA key
with a keysize larger than 512 bits is installed.

SSL_set_tmp_rsa_callback() sets the callback only for B<ssl>.

SSL_set_tmp_rsa() sets the key only for B<ssl>.

SSL_need_tmp_rsa() returns 1, if a temporary/ephemeral RSA key is needed,
for RSA-based strength-limited 'exportable' ciphersuites because a RSA key
with a keysize larger than 512 bits is installed.

These functions apply to SSL/TLS servers only.

=head1 NOTES

When using a cipher with RSA authentication, an ephemeral RSA key exchange
can take place. In this case the session data are negotiated using the
ephemeral/temporary RSA key and the RSA key supplied and certified
by the certificate chain is only used for signing.

Under previous export restrictions, ciphers with RSA keys shorter (512 bits)
than the usual key length of 1024 bits were created. To use these ciphers
with RSA keys of usual length, an ephemeral key exchange must be performed,
as the normal (certified) key cannot be directly used.

Using ephemeral RSA key exchange yields forward secrecy, as the connection
can only be decrypted, when the RSA key is known. By generating a temporary
RSA key inside the server application that is lost when the application
is left, it becomes impossible for an attacker to decrypt past sessions,
even if he gets hold of the normal (certified) RSA key, as this key was
used for signing only. The downside is that creating a RSA key is
computationally expensive.

Additionally, the use of ephemeral RSA key exchange is only allowed in
the TLS standard, when the RSA key can be used for signing only, that is
for export ciphers. Using ephemeral RSA key exchange for other purposes
violates the standard and can break interoperability with clients.
It is therefore strongly recommended to not use ephemeral RSA key
exchange and use EDH (Ephemeral Diffie-Hellman) key exchange instead
in order to achieve forward secrecy (see
L<SSL_CTX_set_tmp_dh_callback(3)|SSL_CTX_set_tmp_dh_callback(3)>).

An application may either directly specify the key or can supply the key via a
callback function. The callback approach has the advantage, that the callback
may generate the key only in case it is actually needed. As the generation of a
RSA key is however costly, it will lead to a significant delay in the handshake
procedure.  Another advantage of the callback function is that it can supply
keys of different size while the explicit setting of the key is only useful for
key size of 512 bits to satisfy the export restricted ciphers and does give
away key length if a longer key would be allowed.

The B<tmp_rsa_callback> is called with the B<keylength> needed and
the B<is_export> information. The B<is_export> flag is set, when the
ephemeral RSA key exchange is performed with an export cipher.

=head1 EXAMPLES

Generate temporary RSA keys to prepare ephemeral RSA key exchange. As the
generation of a RSA key costs a lot of computer time, they saved for later
reuse. For demonstration purposes, two keys for 512 bits and 1024 bits
respectively are generated.

 ...
 /* Set up ephemeral RSA stuff */
 RSA *rsa_512 = NULL;
 RSA *rsa_1024 = NULL;

 rsa_512 = RSA_generate_key(512,RSA_F4,NULL,NULL);
 if (rsa_512 == NULL)
     evaluate_error_queue();

 rsa_1024 = RSA_generate_key(1024,RSA_F4,NULL,NULL);
 if (rsa_1024 == NULL)
   evaluate_error_queue();

 ...

 RSA *tmp_rsa_callback(SSL *s, int is_export, int keylength)
 {
    RSA *rsa_tmp=NULL;

    switch (keylength) {
    case 512:
      if (rsa_512)
        rsa_tmp = rsa_512;
      else { /* generate on the fly, should not happen in this example */
        rsa_tmp = RSA_generate_key(keylength,RSA_F4,NULL,NULL);
        rsa_512 = rsa_tmp; /* Remember for later reuse */
      }
      break;
    case 1024:
      if (rsa_1024)
        rsa_tmp=rsa_1024;
      else
        should_not_happen_in_this_example();
      break;
    default:
      /* Generating a key on the fly is very costly, so use what is there */
      if (rsa_1024)
        rsa_tmp=rsa_1024;
      else
        rsa_tmp=rsa_512; /* Use at least a shorter key */
    }
    return(rsa_tmp);
 }

=head1 RETURN VALUES

SSL_CTX_set_tmp_rsa_callback() and SSL_set_tmp_rsa_callback() do not return
diagnostic output.

SSL_CTX_set_tmp_rsa() and SSL_set_tmp_rsa() do return 1 on success and 0
on failure. Check the error queue to find out the reason of failure.

SSL_CTX_need_tmp_rsa() and SSL_need_tmp_rsa() return 1 if a temporary
RSA key is needed and 0 otherwise.

=head1 SEE ALSO

L<ssl(3)|ssl(3)>, L<SSL_CTX_set_cipher_list(3)|SSL_CTX_set_cipher_list(3)>,
L<SSL_CTX_set_options(3)|SSL_CTX_set_options(3)>,
L<SSL_CTX_set_tmp_dh_callback(3)|SSL_CTX_set_tmp_dh_callback(3)>,
L<SSL_new(3)|SSL_new(3)>, L<ciphers(1)|ciphers(1)>

=cut
