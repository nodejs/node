
 OpenSSL 1.0.1g 7 Apr 2014

 Copyright (c) 1998-2011 The OpenSSL Project
 Copyright (c) 1995-1998 Eric A. Young, Tim J. Hudson
 All rights reserved.

 DESCRIPTION
 -----------

 The OpenSSL Project is a collaborative effort to develop a robust,
 commercial-grade, fully featured, and Open Source toolkit implementing the
 Secure Sockets Layer (SSL v2/v3) and Transport Layer Security (TLS v1)
 protocols as well as a full-strength general purpose cryptography library.
 The project is managed by a worldwide community of volunteers that use the
 Internet to communicate, plan, and develop the OpenSSL toolkit and its
 related documentation.

 OpenSSL is based on the excellent SSLeay library developed from Eric A. Young
 and Tim J. Hudson.  The OpenSSL toolkit is licensed under a dual-license (the
 OpenSSL license plus the SSLeay license) situation, which basically means
 that you are free to get and use it for commercial and non-commercial
 purposes as long as you fulfill the conditions of both licenses.

 OVERVIEW
 --------

 The OpenSSL toolkit includes:

 libssl.a:
     Implementation of SSLv2, SSLv3, TLSv1 and the required code to support
     both SSLv2, SSLv3 and TLSv1 in the one server and client.

 libcrypto.a:
     General encryption and X.509 v1/v3 stuff needed by SSL/TLS but not
     actually logically part of it. It includes routines for the following:

     Ciphers
        libdes - EAY's libdes DES encryption package which was floating
                 around the net for a few years, and was then relicensed by
                 him as part of SSLeay.  It includes 15 'modes/variations'
                 of DES (1, 2 and 3 key versions of ecb, cbc, cfb and ofb;
                 pcbc and a more general form of cfb and ofb) including desx
                 in cbc mode, a fast crypt(3), and routines to read
                 passwords from the keyboard.
        RC4 encryption,
        RC2 encryption      - 4 different modes, ecb, cbc, cfb and ofb.
        Blowfish encryption - 4 different modes, ecb, cbc, cfb and ofb.
        IDEA encryption     - 4 different modes, ecb, cbc, cfb and ofb.

     Digests
        MD5 and MD2 message digest algorithms, fast implementations,
        SHA (SHA-0) and SHA-1 message digest algorithms,
        MDC2 message digest. A DES based hash that is popular on smart cards.

     Public Key
        RSA encryption/decryption/generation.
            There is no limit on the number of bits.
        DSA encryption/decryption/generation.
            There is no limit on the number of bits.
        Diffie-Hellman key-exchange/key generation.
            There is no limit on the number of bits.

     X.509v3 certificates
        X509 encoding/decoding into/from binary ASN1 and a PEM
             based ASCII-binary encoding which supports encryption with a
             private key.  Program to generate RSA and DSA certificate
             requests and to generate RSA and DSA certificates.

     Systems
        The normal digital envelope routines and base64 encoding.  Higher
        level access to ciphers and digests by name.  New ciphers can be
        loaded at run time.  The BIO io system which is a simple non-blocking
        IO abstraction.  Current methods supported are file descriptors,
        sockets, socket accept, socket connect, memory buffer, buffering, SSL
        client/server, file pointer, encryption, digest, non-blocking testing
        and null.

     Data structures
        A dynamically growing hashing system
        A simple stack.
        A Configuration loader that uses a format similar to MS .ini files.

 openssl:
     A command line tool that can be used for:
        Creation of RSA, DH and DSA key parameters
        Creation of X.509 certificates, CSRs and CRLs
        Calculation of Message Digests
        Encryption and Decryption with Ciphers
        SSL/TLS Client and Server Tests
        Handling of S/MIME signed or encrypted mail


 PATENTS
 -------

 Various companies hold various patents for various algorithms in various
 locations around the world. _YOU_ are responsible for ensuring that your use
 of any algorithms is legal by checking if there are any patents in your
 country.  The file contains some of the patents that we know about or are
 rumored to exist. This is not a definitive list.

 RSA Security holds software patents on the RC5 algorithm.  If you
 intend to use this cipher, you must contact RSA Security for
 licensing conditions. Their web page is http://www.rsasecurity.com/.

 RC4 is a trademark of RSA Security, so use of this label should perhaps
 only be used with RSA Security's permission.

 The IDEA algorithm is patented by Ascom in Austria, France, Germany, Italy,
 Japan, the Netherlands, Spain, Sweden, Switzerland, UK and the USA.  They
 should be contacted if that algorithm is to be used; their web page is
 http://www.ascom.ch/.

 NTT and Mitsubishi have patents and pending patents on the Camellia
 algorithm, but allow use at no charge without requiring an explicit
 licensing agreement: http://info.isl.ntt.co.jp/crypt/eng/info/chiteki.html

 INSTALLATION
 ------------

 To install this package under a Unix derivative, read the INSTALL file.  For
 a Win32 platform, read the INSTALL.W32 file.  For OpenVMS systems, read
 INSTALL.VMS.

 Read the documentation in the doc/ directory.  It is quite rough, but it
 lists the functions; you will probably have to look at the code to work out
 how to use them. Look at the example programs.

 PROBLEMS
 --------

 For some platforms, there are some known problems that may affect the user
 or application author.  We try to collect those in doc/PROBLEMS, with current
 thoughts on how they should be solved in a future of OpenSSL.

 SUPPORT
 -------

 See the OpenSSL website www.openssl.org for details of how to obtain
 commercial technical support.

 If you have any problems with OpenSSL then please take the following steps
 first:

    - Download the current snapshot from ftp://ftp.openssl.org/snapshot/
      to see if the problem has already been addressed
    - Remove ASM versions of libraries
    - Remove compiler optimisation flags

 If you wish to report a bug then please include the following information in
 any bug report:

    - On Unix systems:
        Self-test report generated by 'make report'
    - On other systems:
        OpenSSL version: output of 'openssl version -a'
        OS Name, Version, Hardware platform
        Compiler Details (name, version)
    - Application Details (name, version)
    - Problem Description (steps that will reproduce the problem, if known)
    - Stack Traceback (if the application dumps core)

 Report the bug to the OpenSSL project via the Request Tracker
 (http://www.openssl.org/support/rt.html) by mail to:

    openssl-bugs@openssl.org

 Note that the request tracker should NOT be used for general assistance
 or support queries. Just because something doesn't work the way you expect
 does not mean it is necessarily a bug in OpenSSL.

 Note that mail to openssl-bugs@openssl.org is recorded in the publicly
 readable request tracker database and is forwarded to a public
 mailing list. Confidential mail may be sent to openssl-security@openssl.org
 (PGP key available from the key servers).

 HOW TO CONTRIBUTE TO OpenSSL
 ----------------------------

 Development is coordinated on the openssl-dev mailing list (see
 http://www.openssl.org for information on subscribing). If you
 would like to submit a patch, send it to openssl-bugs@openssl.org with
 the string "[PATCH]" in the subject. Please be sure to include a
 textual explanation of what your patch does.

 If you are unsure as to whether a feature will be useful for the general
 OpenSSL community please discuss it on the openssl-dev mailing list first.
 Someone may be already working on the same thing or there may be a good
 reason as to why that feature isn't implemented.

 Patches should be as up to date as possible, preferably relative to the
 current Git or the last snapshot. They should follow the coding style of
 OpenSSL and compile without warnings. Some of the core team developer targets
 can be used for testing purposes, (debug-steve64, debug-geoff etc). OpenSSL
 compiles on many varied platforms: try to ensure you only use portable
 features.

 Note: For legal reasons, contributions from the US can be accepted only
 if a TSU notification and a copy of the patch are sent to crypt@bis.doc.gov
 (formerly BXA) with a copy to the ENC Encryption Request Coordinator;
 please take some time to look at
    http://www.bis.doc.gov/Encryption/PubAvailEncSourceCodeNofify.html [sic]
 and
    http://w3.access.gpo.gov/bis/ear/pdf/740.pdf (EAR Section 740.13(e))
 for the details. If "your encryption source code is too large to serve as
 an email attachment", they are glad to receive it by fax instead; hope you
 have a cheap long-distance plan.

 Our preferred format for changes is "diff -u" output. You might
 generate it like this:

 # cd openssl-work
 # [your changes]
 # ./Configure dist; make clean
 # cd ..
 # diff -ur openssl-orig openssl-work > mydiffs.patch

