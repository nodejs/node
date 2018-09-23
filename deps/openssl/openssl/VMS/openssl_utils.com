$!
$!  APPS.COM
$!  Written By:  Robert Byer
$!               Vice-President
$!               A-Com Computing, Inc.
$!               byer@mail.all-net.net
$!
$!
$! Slightly modified by Richard Levitte <richard@levitte.org>
$!
$!
$! Always define OPENSSL.  Others are optional (non-null P1).
$!
$ OPENSSL  :== $SSLEXE:OPENSSL
$
$ IF (P1 .NES. "")
$ THEN
$     VERIFY   :== $SSLEXE:OPENSSL VERIFY
$     ASN1PARSE:== $SSLEXE:OPENSSL ASN1PARS
$! REQ could conflict with REQUEST.
$     OREQ     :== $SSLEXE:OPENSSL REQ
$     DGST     :== $SSLEXE:OPENSSL DGST
$     DH       :== $SSLEXE:OPENSSL DH
$     ENC      :== $SSLEXE:OPENSSL ENC
$     GENDH    :== $SSLEXE:OPENSSL GENDH
$     ERRSTR   :== $SSLEXE:OPENSSL ERRSTR
$     CA       :== $SSLEXE:OPENSSL CA
$     CRL      :== $SSLEXE:OPENSSL CRL
$     RSA      :== $SSLEXE:OPENSSL RSA
$     DSA      :== $SSLEXE:OPENSSL DSA
$     DSAPARAM :== $SSLEXE:OPENSSL DSAPARAM
$     X509     :== $SSLEXE:OPENSSL X509
$     GENRSA   :== $SSLEXE:OPENSSL GENRSA
$     GENDSA   :== $SSLEXE:OPENSSL GENDSA
$     S_SERVER :== $SSLEXE:OPENSSL S_SERVER
$     S_CLIENT :== $SSLEXE:OPENSSL S_CLIENT
$     SPEED    :== $SSLEXE:OPENSSL SPEED
$     S_TIME   :== $SSLEXE:OPENSSL S_TIME
$     VERSION  :== $SSLEXE:OPENSSL VERSION
$     PKCS7    :== $SSLEXE:OPENSSL PKCS7
$     CRL2PKCS7:== $SSLEXE:OPENSSL CRL2P7
$     SESS_ID  :== $SSLEXE:OPENSSL SESS_ID
$     CIPHERS  :== $SSLEXE:OPENSSL CIPHERS
$     NSEQ     :== $SSLEXE:OPENSSL NSEQ
$     PKCS12   :== $SSLEXE:OPENSSL PKCS12
$ ENDIF
