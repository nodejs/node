# Crypto

    Stability: 2 - Unstable; API changes are being discussed for
    future versions.  Breaking changes will be minimized.  See below.

Use `require('crypto')` to access this module.

The crypto module offers a way of encapsulating secure credentials to be
used as part of a secure HTTPS net or http connection.

It also offers a set of wrappers for OpenSSL's hash, hmac, cipher,
decipher, sign and verify methods.


## crypto.setEngine(engine[, flags])

Load and set engine for some/all OpenSSL functions (selected by flags).

`engine` could be either an id or a path to the engine's shared library.

`flags` is optional and has `ENGINE_METHOD_ALL` value by default. It could take
one of or mix of following flags (defined in `constants` module):

* `ENGINE_METHOD_RSA`
* `ENGINE_METHOD_DSA`
* `ENGINE_METHOD_DH`
* `ENGINE_METHOD_RAND`
* `ENGINE_METHOD_ECDH`
* `ENGINE_METHOD_ECDSA`
* `ENGINE_METHOD_CIPHERS`
* `ENGINE_METHOD_DIGESTS`
* `ENGINE_METHOD_STORE`
* `ENGINE_METHOD_PKEY_METH`
* `ENGINE_METHOD_PKEY_ASN1_METH`
* `ENGINE_METHOD_ALL`
* `ENGINE_METHOD_NONE`


## crypto.getCiphers()

Returns an array with the names of the supported ciphers.

Example:

    var ciphers = crypto.getCiphers();
    console.log(ciphers); // ['AES-128-CBC', 'AES-128-CBC-HMAC-SHA1', ...]


## crypto.getHashes()

Returns an array with the names of the supported hash algorithms.

Example:

    var hashes = crypto.getHashes();
    console.log(hashes); // ['sha', 'sha1', 'sha1WithRSAEncryption', ...]


## crypto.createCredentials(details)

    Stability: 0 - Deprecated. Use [tls.createSecureContext][] instead.

Creates a credentials object, with the optional details being a
dictionary with keys:

* `pfx` : A string or buffer holding the PFX or PKCS12 encoded private
  key, certificate and CA certificates
* `key` : A string holding the PEM encoded private key
* `passphrase` : A string of passphrase for the private key or pfx
* `cert` : A string holding the PEM encoded certificate
* `ca` : Either a string or list of strings of PEM encoded CA
  certificates to trust.
* `crl` : Either a string or list of strings of PEM encoded CRLs
  (Certificate Revocation List)
* `ciphers`: A string describing the ciphers to use or exclude.
  Consult
  <http://www.openssl.org/docs/apps/ciphers.html#CIPHER_LIST_FORMAT>
  for details on the format.

If no 'ca' details are given, then io.js will use the default
publicly trusted list of CAs as given in
<http://mxr.mozilla.org/mozilla/source/security/nss/lib/ckfw/builtins/certdata.txt>.


## crypto.createHash(algorithm)

Creates and returns a hash object, a cryptographic hash with the given
algorithm which can be used to generate hash digests.

`algorithm` is dependent on the available algorithms supported by the
version of OpenSSL on the platform. Examples are `'sha1'`, `'md5'`,
`'sha256'`, `'sha512'`, etc.  On recent releases, `openssl
list-message-digest-algorithms` will display the available digest
algorithms.

Example: this program that takes the sha1 sum of a file

    var filename = process.argv[2];
    var crypto = require('crypto');
    var fs = require('fs');

    var shasum = crypto.createHash('sha1');

    var s = fs.ReadStream(filename);
    s.on('data', function(d) {
      shasum.update(d);
    });

    s.on('end', function() {
      var d = shasum.digest('hex');
      console.log(d + '  ' + filename);
    });

## Class: Hash

The class for creating hash digests of data.

It is a [stream](stream.html) that is both readable and writable.  The
written data is used to compute the hash.  Once the writable side of
the stream is ended, use the `read()` method to get the computed hash
digest.  The legacy `update` and `digest` methods are also supported.

Returned by `crypto.createHash`.

### hash.update(data[, input_encoding])

Updates the hash content with the given `data`, the encoding of which
is given in `input_encoding` and can be `'utf8'`, `'ascii'` or
`'binary'`.  If no encoding is provided and the input is a string an
encoding of `'binary'` is enforced. If `data` is a `Buffer` then
`input_encoding` is ignored.

This can be called many times with new data as it is streamed.

### hash.digest([encoding])

Calculates the digest of all of the passed data to be hashed.  The
`encoding` can be `'hex'`, `'binary'` or `'base64'`.  If no encoding
is provided, then a buffer is returned.

Note: `hash` object can not be used after `digest()` method has been
called.


## crypto.createHmac(algorithm, key)

Creates and returns a hmac object, a cryptographic hmac with the given
algorithm and key.

It is a [stream](stream.html) that is both readable and writable.  The
written data is used to compute the hmac.  Once the writable side of
the stream is ended, use the `read()` method to get the computed
digest.  The legacy `update` and `digest` methods are also supported.

`algorithm` is dependent on the available algorithms supported by
OpenSSL - see createHash above.  `key` is the hmac key to be used.

## Class: Hmac

Class for creating cryptographic hmac content.

Returned by `crypto.createHmac`.

### hmac.update(data)

Update the hmac content with the given `data`.  This can be called
many times with new data as it is streamed.

### hmac.digest([encoding])

Calculates the digest of all of the passed data to the hmac.  The
`encoding` can be `'hex'`, `'binary'` or `'base64'`.  If no encoding
is provided, then a buffer is returned.

Note: `hmac` object can not be used after `digest()` method has been
called.


## crypto.createCipher(algorithm, password)

Creates and returns a cipher object, with the given algorithm and
password.

`algorithm` is dependent on OpenSSL, examples are `'aes192'`, etc.  On
recent releases, `openssl list-cipher-algorithms` will display the
available cipher algorithms.  `password` is used to derive key and IV,
which must be a `'binary'` encoded string or a [buffer](buffer.html).

It is a [stream](stream.html) that is both readable and writable.  The
written data is used to compute the hash.  Once the writable side of
the stream is ended, use the `read()` method to get the enciphered
contents.  The legacy `update` and `final` methods are also supported.

Note: `createCipher` derives keys with the OpenSSL function [EVP_BytesToKey][]
with the digest algorithm set to MD5, one iteration, and no salt. The lack of
salt allows dictionary attacks as the same password always creates the same key.
The low iteration count and non-cryptographically secure hash algorithm allow
passwords to be tested very rapidly.

In line with OpenSSL's recommendation to use pbkdf2 instead of EVP_BytesToKey it
is recommended you derive a key and iv yourself with [crypto.pbkdf2][] and to
then use [createCipheriv()][] to create the cipher stream.

## crypto.createCipheriv(algorithm, key, iv)

Creates and returns a cipher object, with the given algorithm, key and
iv.

`algorithm` is the same as the argument to `createCipher()`.  `key` is
the raw key used by the algorithm.  `iv` is an [initialization
vector](http://en.wikipedia.org/wiki/Initialization_vector).

`key` and `iv` must be `'binary'` encoded strings or
[buffers](buffer.html).

## Class: Cipher

Class for encrypting data.

Returned by `crypto.createCipher` and `crypto.createCipheriv`.

Cipher objects are [streams](stream.html) that are both readable and
writable.  The written plain text data is used to produce the
encrypted data on the readable side.  The legacy `update` and `final`
methods are also supported.

### cipher.update(data[, input_encoding][, output_encoding])

Updates the cipher with `data`, the encoding of which is given in
`input_encoding` and can be `'utf8'`, `'ascii'` or `'binary'`.  If no
encoding is provided, then a buffer is expected.
If `data` is a `Buffer` then `input_encoding` is ignored.

The `output_encoding` specifies the output format of the enciphered
data, and can be `'binary'`, `'base64'` or `'hex'`.  If no encoding is
provided, then a buffer is returned.

Returns the enciphered contents, and can be called many times with new
data as it is streamed.

### cipher.final([output_encoding])

Returns any remaining enciphered contents, with `output_encoding`
being one of: `'binary'`, `'base64'` or `'hex'`.  If no encoding is
provided, then a buffer is returned.

Note: `cipher` object can not be used after `final()` method has been
called.

### cipher.setAutoPadding(auto_padding=true)

You can disable automatic padding of the input data to block size. If
`auto_padding` is false, the length of the entire input data must be a
multiple of the cipher's block size or `final` will fail.  Useful for
non-standard padding, e.g. using `0x0` instead of PKCS padding. You
must call this before `cipher.final`.

### cipher.getAuthTag()

For authenticated encryption modes (currently supported: GCM), this
method returns a `Buffer` that represents the _authentication tag_ that
has been computed from the given data. Should be called after
encryption has been completed using the `final` method!

### cipher.setAAD(buffer)

For authenticated encryption modes (currently supported: GCM), this
method sets the value used for the additional authenticated data (AAD) input
parameter.


## crypto.createDecipher(algorithm, password)

Creates and returns a decipher object, with the given algorithm and
key.  This is the mirror of the [createCipher()][] above.

## crypto.createDecipheriv(algorithm, key, iv)

Creates and returns a decipher object, with the given algorithm, key
and iv.  This is the mirror of the [createCipheriv()][] above.

## Class: Decipher

Class for decrypting data.

Returned by `crypto.createDecipher` and `crypto.createDecipheriv`.

Decipher objects are [streams](stream.html) that are both readable and
writable.  The written enciphered data is used to produce the
plain-text data on the the readable side.  The legacy `update` and
`final` methods are also supported.

### decipher.update(data[, input_encoding][, output_encoding])

Updates the decipher with `data`, which is encoded in `'binary'`,
`'base64'` or `'hex'`.  If no encoding is provided, then a buffer is
expected.
If `data` is a `Buffer` then `input_encoding` is ignored.

The `output_decoding` specifies in what format to return the
deciphered plaintext: `'binary'`, `'ascii'` or `'utf8'`.  If no
encoding is provided, then a buffer is returned.

### decipher.final([output_encoding])

Returns any remaining plaintext which is deciphered, with
`output_encoding` being one of: `'binary'`, `'ascii'` or `'utf8'`.  If
no encoding is provided, then a buffer is returned.

Note: `decipher` object can not be used after `final()` method has been
called.

### decipher.setAutoPadding(auto_padding=true)

You can disable auto padding if the data has been encrypted without
standard block padding to prevent `decipher.final` from checking and
removing it. Can only work if the input data's length is a multiple of
the ciphers block size. You must call this before streaming data to
`decipher.update`.

### decipher.setAuthTag(buffer)

For authenticated encryption modes (currently supported: GCM), this
method must be used to pass in the received _authentication tag_.
If no tag is provided or if the ciphertext has been tampered with,
`final` will throw, thus indicating that the ciphertext should
be discarded due to failed authentication.

### decipher.setAAD(buffer)

For authenticated encryption modes (currently supported: GCM), this
method sets the value used for the additional authenticated data (AAD) input
parameter.


## crypto.createSign(algorithm)

Creates and returns a signing object, with the given algorithm.  On
recent OpenSSL releases, `openssl list-public-key-algorithms` will
display the available signing algorithms. Examples are `'RSA-SHA256'`.

## Class: Sign

Class for generating signatures.

Returned by `crypto.createSign`.

Sign objects are writable [streams](stream.html).  The written data is
used to generate the signature.  Once all of the data has been
written, the `sign` method will return the signature.  The legacy
`update` method is also supported.

### sign.update(data)

Updates the sign object with data.  This can be called many times
with new data as it is streamed.

### sign.sign(private_key[, output_format])

Calculates the signature on all the updated data passed through the
sign.

`private_key` can be an object or a string. If `private_key` is a string, it is
treated as the key with no passphrase.

`private_key`:

* `key` : A string holding the PEM encoded private key
* `passphrase` : A string of passphrase for the private key

Returns the signature in `output_format` which can be `'binary'`,
`'hex'` or `'base64'`. If no encoding is provided, then a buffer is
returned.

Note: `sign` object can not be used after `sign()` method has been
called.

## crypto.createVerify(algorithm)

Creates and returns a verification object, with the given algorithm.
This is the mirror of the signing object above.

## Class: Verify

Class for verifying signatures.

Returned by `crypto.createVerify`.

Verify objects are writable [streams](stream.html).  The written data
is used to validate against the supplied signature.  Once all of the
data has been written, the `verify` method will return true if the
supplied signature is valid.  The legacy `update` method is also
supported.

### verifier.update(data)

Updates the verifier object with data.  This can be called many times
with new data as it is streamed.

### verifier.verify(object, signature[, signature_format])

Verifies the signed data by using the `object` and `signature`.
`object` is  a string containing a PEM encoded object, which can be
one of RSA public key, DSA public key, or X.509 certificate.
`signature` is the previously calculated signature for the data, in
the `signature_format` which can be `'binary'`, `'hex'` or `'base64'`.
If no encoding is specified, then a buffer is expected.

Returns true or false depending on the validity of the signature for
the data and public key.

Note: `verifier` object can not be used after `verify()` method has been
called.

## crypto.createDiffieHellman(prime_length[, generator])

Creates a Diffie-Hellman key exchange object and generates a prime of
`prime_length` bits and using an optional specific numeric `generator`.
If no `generator` is specified, then `2` is used.

## crypto.createDiffieHellman(prime[, prime_encoding][, generator][, generator_encoding])

Creates a Diffie-Hellman key exchange object using the supplied `prime` and an
optional specific `generator`.
`generator` can be a number, string, or Buffer.
If no `generator` is specified, then `2` is used.
`prime_encoding` and `generator_encoding` can be `'binary'`, `'hex'`, or `'base64'`.
If no `prime_encoding` is specified, then a Buffer is expected for `prime`.
If no `generator_encoding` is specified, then a Buffer is expected for `generator`.

## Class: DiffieHellman

The class for creating Diffie-Hellman key exchanges.

Returned by `crypto.createDiffieHellman`.

### diffieHellman.verifyError

A bit field containing any warnings and/or errors as a result of a check performed
during initialization. The following values are valid for this property
(defined in `constants` module):

* `DH_CHECK_P_NOT_SAFE_PRIME`
* `DH_CHECK_P_NOT_PRIME`
* `DH_UNABLE_TO_CHECK_GENERATOR`
* `DH_NOT_SUITABLE_GENERATOR`

### diffieHellman.generateKeys([encoding])

Generates private and public Diffie-Hellman key values, and returns
the public key in the specified encoding. This key should be
transferred to the other party. Encoding can be `'binary'`, `'hex'`,
or `'base64'`.  If no encoding is provided, then a buffer is returned.

### diffieHellman.computeSecret(other_public_key[, input_encoding][, output_encoding])

Computes the shared secret using `other_public_key` as the other
party's public key and returns the computed shared secret. Supplied
key is interpreted using specified `input_encoding`, and secret is
encoded using specified `output_encoding`. Encodings can be
`'binary'`, `'hex'`, or `'base64'`. If the input encoding is not
provided, then a buffer is expected.

If no output encoding is given, then a buffer is returned.

### diffieHellman.getPrime([encoding])

Returns the Diffie-Hellman prime in the specified encoding, which can
be `'binary'`, `'hex'`, or `'base64'`. If no encoding is provided,
then a buffer is returned.

### diffieHellman.getGenerator([encoding])

Returns the Diffie-Hellman generator in the specified encoding, which can
be `'binary'`, `'hex'`, or `'base64'`. If no encoding is provided,
then a buffer is returned.

### diffieHellman.getPublicKey([encoding])

Returns the Diffie-Hellman public key in the specified encoding, which
can be `'binary'`, `'hex'`, or `'base64'`. If no encoding is provided,
then a buffer is returned.

### diffieHellman.getPrivateKey([encoding])

Returns the Diffie-Hellman private key in the specified encoding,
which can be `'binary'`, `'hex'`, or `'base64'`. If no encoding is
provided, then a buffer is returned.

### diffieHellman.setPublicKey(public_key[, encoding])

Sets the Diffie-Hellman public key. Key encoding can be `'binary'`,
`'hex'` or `'base64'`. If no encoding is provided, then a buffer is
expected.

### diffieHellman.setPrivateKey(private_key[, encoding])

Sets the Diffie-Hellman private key. Key encoding can be `'binary'`,
`'hex'` or `'base64'`. If no encoding is provided, then a buffer is
expected.

## crypto.getDiffieHellman(group_name)

Creates a predefined Diffie-Hellman key exchange object.  The
supported groups are: `'modp1'`, `'modp2'`, `'modp5'` (defined in [RFC
2412][]) and `'modp14'`, `'modp15'`, `'modp16'`, `'modp17'`,
`'modp18'` (defined in [RFC 3526][]).  The returned object mimics the
interface of objects created by [crypto.createDiffieHellman()][]
above, but will not allow to change the keys (with
[diffieHellman.setPublicKey()][] for example).  The advantage of using
this routine is that the parties don't have to generate nor exchange
group modulus beforehand, saving both processor and communication
time.

Example (obtaining a shared secret):

    var crypto = require('crypto');
    var alice = crypto.getDiffieHellman('modp5');
    var bob = crypto.getDiffieHellman('modp5');

    alice.generateKeys();
    bob.generateKeys();

    var alice_secret = alice.computeSecret(bob.getPublicKey(), null, 'hex');
    var bob_secret = bob.computeSecret(alice.getPublicKey(), null, 'hex');

    /* alice_secret and bob_secret should be the same */
    console.log(alice_secret == bob_secret);

## crypto.createECDH(curve_name)

Creates a Elliptic Curve (EC) Diffie-Hellman key exchange object using a
predefined curve specified by `curve_name` string.

## Class: ECDH

The class for creating EC Diffie-Hellman key exchanges.

Returned by `crypto.createECDH`.

### ECDH.generateKeys([encoding[, format]])

Generates private and public EC Diffie-Hellman key values, and returns
the public key in the specified format and encoding. This key should be
transferred to the other party.

Format specifies point encoding and can be `'compressed'`, `'uncompressed'`, or
`'hybrid'`. If no format is provided - the point will be returned in
`'uncompressed'` format.

Encoding can be `'binary'`, `'hex'`, or `'base64'`. If no encoding is provided,
then a buffer is returned.

### ECDH.computeSecret(other_public_key[, input_encoding][, output_encoding])

Computes the shared secret using `other_public_key` as the other
party's public key and returns the computed shared secret. Supplied
key is interpreted using specified `input_encoding`, and secret is
encoded using specified `output_encoding`. Encodings can be
`'binary'`, `'hex'`, or `'base64'`. If the input encoding is not
provided, then a buffer is expected.

If no output encoding is given, then a buffer is returned.

### ECDH.getPublicKey([encoding[, format]])

Returns the EC Diffie-Hellman public key in the specified encoding and format.

Format specifies point encoding and can be `'compressed'`, `'uncompressed'`, or
`'hybrid'`. If no format is provided - the point will be returned in
`'uncompressed'` format.

Encoding can be `'binary'`, `'hex'`, or `'base64'`. If no encoding is provided,
then a buffer is returned.

### ECDH.getPrivateKey([encoding])

Returns the EC Diffie-Hellman private key in the specified encoding,
which can be `'binary'`, `'hex'`, or `'base64'`. If no encoding is
provided, then a buffer is returned.

### ECDH.setPublicKey(public_key[, encoding])

Sets the EC Diffie-Hellman public key. Key encoding can be `'binary'`,
`'hex'` or `'base64'`. If no encoding is provided, then a buffer is
expected.

### ECDH.setPrivateKey(private_key[, encoding])

Sets the EC Diffie-Hellman private key. Key encoding can be `'binary'`,
`'hex'` or `'base64'`. If no encoding is provided, then a buffer is
expected.

Example (obtaining a shared secret):

    var crypto = require('crypto');
    var alice = crypto.createECDH('secp256k1');
    var bob = crypto.createECDH('secp256k1');

    alice.generateKeys();
    bob.generateKeys();

    var alice_secret = alice.computeSecret(bob.getPublicKey(), null, 'hex');
    var bob_secret = bob.computeSecret(alice.getPublicKey(), null, 'hex');

    /* alice_secret and bob_secret should be the same */
    console.log(alice_secret == bob_secret);

## crypto.pbkdf2(password, salt, iterations, keylen[, digest], callback)

Asynchronous PBKDF2 function.  Applies the selected HMAC digest function
(default: SHA1) to derive a key of the requested length from the password,
salt and number of iterations.  The callback gets two arguments:
`(err, derivedKey)`.

Example:

    crypto.pbkdf2('secret', 'salt', 4096, 512, 'sha256', function(err, key) {
      if (err)
        throw err;
      console.log(key.toString('hex'));  // 'c5e478d...1469e50'
    });

You can get a list of supported digest functions with
[crypto.getHashes()](#crypto_crypto_gethashes).

## crypto.pbkdf2Sync(password, salt, iterations, keylen[, digest])

Synchronous PBKDF2 function.  Returns derivedKey or throws error.

## crypto.randomBytes(size[, callback])

Generates cryptographically strong pseudo-random data. Usage:

    // async
    crypto.randomBytes(256, function(ex, buf) {
      if (ex) throw ex;
      console.log('Have %d bytes of random data: %s', buf.length, buf);
    });

    // sync
    try {
      var buf = crypto.randomBytes(256);
      console.log('Have %d bytes of random data: %s', buf.length, buf);
    } catch (ex) {
      // handle error
      // most likely, entropy sources are drained
    }

NOTE: This will block if there is insufficient entropy, although it should
normally never take longer than a few milliseconds. The only time when this
may conceivably block is right after boot, when the whole system is still
low on entropy.

## Class: Certificate

The class used for working with signed public key & challenges. The most
common usage for this series of functions is when dealing with the `<keygen>`
element. http://www.openssl.org/docs/apps/spkac.html

Returned by `crypto.Certificate`.

### Certificate.verifySpkac(spkac)

Returns true of false based on the validity of the SPKAC.

### Certificate.exportChallenge(spkac)

Exports the encoded public key from the supplied SPKAC.

### Certificate.exportPublicKey(spkac)

Exports the encoded challenge associated with the SPKAC.

## crypto.publicEncrypt(public_key, buffer)

Encrypts `buffer` with `public_key`. Only RSA is currently supported.

`public_key` can be an object or a string. If `public_key` is a string, it is
treated as the key with no passphrase and will use `RSA_PKCS1_OAEP_PADDING`.
Since RSA public keys may be derived from private keys you may pass a private
key to this method.

`public_key`:

* `key` : A string holding the PEM encoded private key
* `passphrase` : An optional string of passphrase for the private key
* `padding` : An optional padding value, one of the following:
  * `constants.RSA_NO_PADDING`
  * `constants.RSA_PKCS1_PADDING`
  * `constants.RSA_PKCS1_OAEP_PADDING`

NOTE: All paddings are defined in `constants` module.

## crypto.publicDecrypt(public_key, buffer)

See above for details. Has the same API as `crypto.publicEncrypt`. Default
padding is `RSA_PKCS1_PADDING`.

## crypto.privateDecrypt(private_key, buffer)

Decrypts `buffer` with `private_key`.

`private_key` can be an object or a string. If `private_key` is a string, it is
treated as the key with no passphrase and will use `RSA_PKCS1_OAEP_PADDING`.

`private_key`:

* `key` : A string holding the PEM encoded private key
* `passphrase` : An optional string of passphrase for the private key
* `padding` : An optional padding value, one of the following:
  * `constants.RSA_NO_PADDING`
  * `constants.RSA_PKCS1_PADDING`
  * `constants.RSA_PKCS1_OAEP_PADDING`

NOTE: All paddings are defined in `constants` module.

## crypto.privateEncrypt(private_key, buffer)

See above for details. Has the same API as `crypto.privateDecrypt`.
Default padding is `RSA_PKCS1_PADDING`.

## crypto.DEFAULT_ENCODING

The default encoding to use for functions that can take either strings
or buffers.  The default value is `'buffer'`, which makes it default
to using Buffer objects.  This is here to make the crypto module more
easily compatible with legacy programs that expected `'binary'` to be
the default encoding.

Note that new programs will probably expect buffers, so only use this
as a temporary measure.

## Recent API Changes

The Crypto module was added to Node.js before there was the concept of a
unified Stream API, and before there were Buffer objects for handling
binary data.

As such, the streaming classes don't have the typical methods found on
other io.js classes, and many methods accepted and returned
Binary-encoded strings by default rather than Buffers.  This was
changed to use Buffers by default instead.

This is a breaking change for some use cases, but not all.

For example, if you currently use the default arguments to the Sign
class, and then pass the results to the Verify class, without ever
inspecting the data, then it will continue to work as before.  Where
you once got a binary string and then presented the binary string to
the Verify object, you'll now get a Buffer, and present the Buffer to
the Verify object.

However, if you were doing things with the string data that will not
work properly on Buffers (such as, concatenating them, storing in
databases, etc.), or you are passing binary strings to the crypto
functions without an encoding argument, then you will need to start
providing encoding arguments to specify which encoding you'd like to
use.  To switch to the previous style of using binary strings by
default, set the `crypto.DEFAULT_ENCODING` field to 'binary'.  Note
that new programs will probably expect buffers, so only use this as a
temporary measure.


[createCipher()]: #crypto_crypto_createcipher_algorithm_password
[createCipheriv()]: #crypto_crypto_createcipheriv_algorithm_key_iv
[crypto.createDiffieHellman()]: #crypto_crypto_creatediffiehellman_prime_encoding
[tls.createSecureContext]: tls.html#tls_tls_createsecurecontext_details
[diffieHellman.setPublicKey()]: #crypto_diffiehellman_setpublickey_public_key_encoding
[RFC 2412]: http://www.rfc-editor.org/rfc/rfc2412.txt
[RFC 3526]: http://www.rfc-editor.org/rfc/rfc3526.txt
[crypto.pbkdf2]: #crypto_crypto_pbkdf2_password_salt_iterations_keylen_callback
[EVP_BytesToKey]: https://www.openssl.org/docs/crypto/EVP_BytesToKey.html
