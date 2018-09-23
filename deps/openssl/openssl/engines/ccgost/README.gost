GOST ENGINE

This engine provides implementation of Russian cryptography standard.
This is also an example of adding new cryptoalgorithms into OpenSSL
without changing its core. If OpenSSL is compiled with dynamic engine
support, new algorithms can be added even without recompilation of
OpenSSL and applications which use it.

ALGORITHMS SUPPORTED

GOST R 34.10-94 and GOST R 34.10-2001 - digital signature algorithms.
   Also support key exchange based on public keys. See RFC 4357 for
   details of VKO key exchange algorithm. These algorithms use
   256 bit private keys. Public keys are 1024 bit for 94 and 512 bit for
   2001 (which is elliptic-curve based). Key exchange algorithms
   (VKO R 34.10) are supported on these keys too.
   
GOST R 34.11-94  Message digest algorithm. 256-bit hash value

GOST 28147-89 - Symmetric cipher  with 256-bit key. Various modes are
   defined in the standard, but only CFB and CNT modes are implemented
   in the engine. To make statistical analysis more difficult, key
   meshing is supported (see RFC 4357).

GOST 28147-89 MAC mode. Message authentication code. While most MAC
    algorithms  out there are based on hash functions using HMAC
	algorithm, this algoritm is based on symmetric cipher. 
	It has 256-bit symmetric key and only 32 bits of MAC value
	(while HMAC has same key size and value size). 

	It is implemented as combination of EVP_PKEY type and EVP_MD type.

USAGE OF THESE ALGORITHMS

This engine is designed to allow usage of this algorithms in the
high-level openssl functions, such as PKI, S/MIME and TLS.

See RFC 4490 for S/MIME with GOST algorithms and RFC 4491 for PKI.
TLS support is implemented according IETF
draft-chudov-cryptopro-cptls-03.txt and is compatible with
CryptoPro CSP 3.0 and 3.6 as well as with MagPro CSP. 
GOST ciphersuites implemented in CryptoPro CSP 2.0 are not supported
because they use ciphersuite numbers used now by AES ciphersuites.

To use the engine you have to load it via openssl configuration
file. Applications should read openssl configuration file or provide
their own means to load engines. Also, applications which operate with
private keys, should use generic EVP_PKEY API instead of using RSA or
other algorithm-specific API.

CONFIGURATION FILE

Configuration file should include following statement in the global
section, i.e. before first bracketed section header (see config(5) for details)

   openssl_conf = openssl_def

where openssl_def is name of the section in configuration file which
describes global defaults.

This section should contain following statement:

   [openssl_def]
   engines = engine_section

which points to the section which describes list of the engines to be
loaded. This section should contain:

	[engine_section]
	gost = gost_section

And section which describes configuration of the engine should contain

	[gost_section]
	engine_id = gost
	dynamic_path = /usr/lib/ssl/engines/libgost.so
	default_algorithms = ALL
	CRYPT_PARAMS = id-Gost28147-89-CryptoPro-A-ParamSet

Where engine_id parameter specifies name of engine (should be "gost").
dynamic_path is a location of the loadable shared library implementing the
engine. If the engine is compiled statically or is located in the OpenSSL
engines directory, this line can be omitted. 
default_algorithms parameter specifies that all algorithms, provided by
engine, should be used.

The CRYPT_PARAMS parameter is engine-specific. It allows the user to choose
between different parameter sets of symmetric cipher algorithm. RFC 4357
specifies several parameters for the GOST 28147-89 algorithm, but OpenSSL
doesn't provide user interface to choose one when encrypting. So use engine
configuration parameter instead.

Value of this parameter can be either short name, defined in OpenSSL
obj_dat.h header file or numeric representation of OID, defined in RFC
4357. 

USAGE WITH COMMAND LINE openssl UTILITY

1. Generation of private key

	openssl genpkey -algorithm gost2001 -pkeyopt paramset:A -out seckey.pem

  Use -algorithm option to specify algorithm.
  Use -pkeyopt option to pass paramset to algorithm. The following paramsets
  are supported by 
  	gost94: 0,A,B,C,D,XA,XB,XC
	gost2001: 0,A,B,C,XA,XB
  You can also use numeric representation of OID as to destinate
  paramset.

  Paramsets starting with X are intended to use for key exchange keys.
  Paramsets without X are for digital signature keys.

  Paramset for both algorithms 0 is the test paramset which should be used
  only for test purposes.

There are no algorithm-specific things with generation of certificate
request once you have a private key.

2. Generation of certificate request along with private/public keypar

   openssl req -newkey gost2001 -pkeyopt paramset:A

   Syntax of -pkeyopt parameter is identical with genpkey command.

   You can also use oldstyle syntax -newkey gost2001:paramfile, but in
   this case you should create parameter file first. 

   It can be created with

   openssl genpkey -genparam -algorithm gost2001 -pkeyopt paramset:A\
      -out paramfile.

3. S/MIME operations

If you want to send encrypted mail using GOST algorithms, don't forget
to specify -gost89 as encryption algorithm for OpenSSL smime command.
While OpenSSL is clever enough to find out that GOST R 34.11-94 digest
must be used for digital signing with GOST private key, it have no way
to derive symmetric encryption algorithm from key exchange keys.

4. TLS operations

OpenSSL supports all four ciphersuites defined in the IETF draft.
Once you've loaded GOST key and certificate into your TLS server,
ciphersuites which use GOST 28147-89 encryption are enabled.

Ciphersuites with NULL encryption should be enabled explicitely if
needed.

GOST2001-GOST89-GOST89 Uses GOST R 34.10-2001 for auth and key exchange
		GOST 28147-89 for encryption and GOST 28147-89 MAC
GOST94-GOST89-GOST89 Uses GOST R 34.10-94 for auth and key exchange
		GOST 28147-89 for encryption and GOST 28147-89 MAC
GOST2001-NULL-GOST94 Uses GOST R 34.10-2001 for auth and key exchange,
        no encryption and HMAC, based on GOST R 34.11-94
GOST94-NULL-GOST94 Uses GOST R 34.10-94 for auth and key exchange,
        no encryption and HMAC, based on GOST R 34.11-94

Gost 94 and gost 2001 keys can be used simultaneously in the TLS server.
RSA, DSA and EC keys can be used simultaneously with GOST keys, if
server implementation supports loading more than two private
key/certificate pairs. In this case ciphersuites which use any of loaded
keys would be supported and clients can negotiate ones they wish.

This allows creation of TLS servers which use GOST ciphersuites for
Russian clients and RSA/DSA ciphersuites for foreign clients.

5. Calculation of digests and symmetric encryption
 OpenSSL provides specific commands (like sha1, aes etc) for calculation
 of digests and symmetric encryption. Since such commands cannot be
 added dynamically, no such commands are provided for GOST algorithms.
 Use generic commands 'dgst' and 'enc'.

 Calculation of GOST R 34.11-94 message digest

 openssl dgst -md_gost94 datafile

 Note that GOST R 34.11-94 specifies that digest value should be
 interpreted as little-endian number, but OpenSSL outputs just hex dump
 of digest value.

 So, to obtain correct digest value, such as produced by gostsum utility
 included in the engine distribution, bytes of output should be
 reversed.
 
 Calculation of HMAC based on GOST R 34.11-94

 openssl dgst -md_gost94 -mac hmac -macopt key:<32 bytes of key> datafile
  
  (or use hexkey if key contain NUL bytes)
 Calculation of GOST 28147 MAC

 openssl dgst -mac gost-mac -macopt key:<32 bytes of key> datafile

 Note absense of an option that specifies digest algorithm. gost-mac
 algorithm supports only one digest (which is actually part of
 implementation of this mac) and OpenSSL is clever enough to find out
 this.

 Encryption with GOST 28147 CFB mode
 openssl enc -gost89 -out encrypted-file -in plain-text-file -k <passphrase>  
 Encryption with GOST 28147 CNT mode
 openssl enc -gost89-cnt -out encrypted-file -in plain-text-file -k <passphrase>


6. Encrypting private keys and PKCS12

To produce PKCS12 files compatible with MagPro CSP, you need to use
GOST algorithm for encryption of PKCS12 file and also GOST R 34.11-94
hash to derive key from password.

openssl pksc12 -export -inkey gost.pem -in gost_cert.pem -keypbe gost89\
   -certpbe gost89 -macalg md_gost94
 
7. Testing speed of symmetric ciphers.
   
To test performance of GOST symmetric ciphers you should use -evp switch
of the openssl speed command. Engine-provided ciphers couldn't be
accessed by cipher-specific functions, only via generic evp interface

 openssl speed -evp gost89
 openssl speed -evp gost89-cnt


PROGRAMMING INTERFACES DETAILS

Applications never should access engine directly. They only use provided
EVP_PKEY API. But there are some details, which should be taken into
account.

EVP provides two kinds of API for key exchange:

1. EVP_PKEY_encrypt/EVP_PKEY_decrypt functions, intended to use with
	RSA-like public key encryption algorithms

2. EVP_PKEY_derive, intended to use with Diffie-Hellman-like shared key
computing algorithms.

Although VKO R 34.10 algorithms, described in the RFC 4357 are
definitely second case, engine provides BOTH API for GOST R 34.10 keys.

EVP_PKEY_derive just invokes appropriate VKO algorithm and computes
256 bit shared key. VKO R 34.10-2001 requires 64 bits of random user key
material (UKM). This UKM should be transmitted to other party, so it is
not generated inside derive function.

It should be set by EVP_PKEY_CTX_ctrl function using
EVP_PKEY_CTRL_SET_IV command after call of EVP_PKEY_derive_init, but
before EVP_PKEY_derive.
	unsigned char ukm[8];
	RAND_bytes(ukm,8);
   EVP_PKEY_CTX_ctrl(ctx, -1, EVP_PKEY_OP_DERIVE, 8, ukm)

EVP_PKEY_encrypt encrypts provided session key with VKO shared key and
packs it into GOST key transport structure, described in the RFC 4490.

It typically uses ephemeral key pair to compute shared key and packs its
public part along with encrypted key. So, for most cases use of 
EVP_PKEY_encrypt/EVP_PKEY_decrypt with GOST keys is almost same as with
RSA.

However, if peerkey field in the EVP_PKEY_CTX structure is set (using
EVP_PKEY_derive_set_peerkey function) to EVP_PKEY structure which has private
key and uses same parameters as the public key from which this EVP_PKEY_CTX is
created, EVP_PKEY_encrypt will use this private key to compute shared key and
set ephemeral key in the GOST_key_transport structure to NULL. In this case
pkey and peerkey fields in the EVP_PKEY_CTX are used upside-down.

If EVP_PKEY_decrypt encounters GOST_key_transport structure with NULL
public key field, it tries to use peerkey field from the context to
compute shared key. In this case peerkey field should really contain
peer public key.

Encrypt operation supports EVP_PKEY_CTRL_SET_IV operation as well.
It can be used when some specific restriction on UKM are imposed by
higher level protocol. For instance, description of GOST ciphersuites
requires UKM to be derived from shared secret. 

If UKM is not set by this control command, encrypt operation would
generate random UKM.


This sources include implementation of GOST 28147-89 and GOST R 34.11-94
which are completely indepentent from OpenSSL and can be used separately
(files gost89.c, gost89.h, gosthash.c, gosthash.h) Utility gostsum (file
gostsum.c) is provided as example of such separate usage. This is
program, simular to md5sum and sha1sum utilities, but calculates GOST R
34.11-94 hash.

Makefile doesn't include rule for compiling gostsum.
Use command

$(CC) -o gostsum gostsum.c gost89.c gosthash.c
where $(CC) is name of your C compiler.

Implementations of GOST R 34.10-xx, including VKO algorithms heavily
depends on OpenSSL BIGNUM and Elliptic Curve libraries.


