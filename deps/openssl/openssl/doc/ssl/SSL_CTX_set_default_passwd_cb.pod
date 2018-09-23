=pod

=head1 NAME

SSL_CTX_set_default_passwd_cb, SSL_CTX_set_default_passwd_cb_userdata - set passwd callback for encrypted PEM file handling

=head1 SYNOPSIS

 #include <openssl/ssl.h>

 void SSL_CTX_set_default_passwd_cb(SSL_CTX *ctx, pem_password_cb *cb);
 void SSL_CTX_set_default_passwd_cb_userdata(SSL_CTX *ctx, void *u);

 int pem_passwd_cb(char *buf, int size, int rwflag, void *userdata);

=head1 DESCRIPTION

SSL_CTX_set_default_passwd_cb() sets the default password callback called
when loading/storing a PEM certificate with encryption.

SSL_CTX_set_default_passwd_cb_userdata() sets a pointer to B<userdata> which
will be provided to the password callback on invocation.

The pem_passwd_cb(), which must be provided by the application, hands back the
password to be used during decryption. On invocation a pointer to B<userdata>
is provided. The pem_passwd_cb must write the password into the provided buffer
B<buf> which is of size B<size>. The actual length of the password must
be returned to the calling function. B<rwflag> indicates whether the
callback is used for reading/decryption (rwflag=0) or writing/encryption
(rwflag=1).

=head1 NOTES

When loading or storing private keys, a password might be supplied to
protect the private key. The way this password can be supplied may depend
on the application. If only one private key is handled, it can be practical
to have pem_passwd_cb() handle the password dialog interactively. If several
keys have to be handled, it can be practical to ask for the password once,
then keep it in memory and use it several times. In the last case, the
password could be stored into the B<userdata> storage and the
pem_passwd_cb() only returns the password already stored.

When asking for the password interactively, pem_passwd_cb() can use
B<rwflag> to check, whether an item shall be encrypted (rwflag=1).
In this case the password dialog may ask for the same password twice
for comparison in order to catch typos, that would make decryption
impossible.

Other items in PEM formatting (certificates) can also be encrypted, it is
however not usual, as certificate information is considered public.

=head1 RETURN VALUES

SSL_CTX_set_default_passwd_cb() and SSL_CTX_set_default_passwd_cb_userdata()
do not provide diagnostic information.

=head1 EXAMPLES

The following example returns the password provided as B<userdata> to the
calling function. The password is considered to be a '\0' terminated
string. If the password does not fit into the buffer, the password is
truncated.

 int pem_passwd_cb(char *buf, int size, int rwflag, void *password)
 {
  strncpy(buf, (char *)(password), size);
  buf[size - 1] = '\0';
  return(strlen(buf));
 }

=head1 SEE ALSO

L<ssl(3)|ssl(3)>,
L<SSL_CTX_use_certificate(3)|SSL_CTX_use_certificate(3)>

=cut
