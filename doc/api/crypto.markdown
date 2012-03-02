# Crypto

    Stability: 3 - Stable

Use `require('crypto')` to access this module.

The crypto module requires OpenSSL to be available on the underlying platform.
It offers a way of encapsulating secure credentials to be used as part
of a secure HTTPS net or http connection.

It also offers a set of wrappers for OpenSSL's hash, hmac, cipher, decipher, sign and verify methods.

## crypto.createCredentials(details)

Creates a credentials object, with the optional details being a dictionary with keys:

* `key` : a string holding the PEM encoded private key
* `cert` : a string holding the PEM encoded certificate
* `ca` : either a string or list of strings of PEM encoded CA certificates to trust.
* `ciphers`: a string describing the ciphers to use or exclude. Consult
  <http://www.openssl.org/docs/apps/ciphers.html#CIPHER_LIST_FORMAT> for details
  on the format.

If no 'ca' details are given, then node.js will use the default publicly trusted list of CAs as given in
<http://mxr.mozilla.org/mozilla/source/security/nss/lib/ckfw/builtins/certdata.txt>.


## crypto.createHash(algorithm)

Creates and returns a hash object, a cryptographic hash with the given algorithm
which can be used to generate hash digests.

`algorithm` is dependent on the available algorithms supported by the version
of OpenSSL on the platform. Examples are `'sha1'`, `'md5'`, `'sha256'`, `'sha512'`, etc.
On recent releases, `openssl list-message-digest-algorithms` will display the available digest algorithms.

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

Returned by `crypto.createHash`.

### hash.update(data, [input_encoding])

Updates the hash content with the given `data`, the encoding of which is given
in `input_encoding` and can be `'utf8'`, `'ascii'` or `'binary'`.
Defaults to `'binary'`.
This can be called many times with new data as it is streamed.

### hash.digest([encoding])

Calculates the digest of all of the passed data to be hashed.
The `encoding` can be `'hex'`, `'binary'` or `'base64'`.
Defaults to `'binary'`.

Note: `hash` object can not be used after `digest()` method been called.


## crypto.createHmac(algorithm, key)

Creates and returns a hmac object, a cryptographic hmac with the given algorithm and key.

`algorithm` is dependent on the available algorithms supported by OpenSSL - see createHash above.
`key` is the hmac key to be used.

## Class: Hmac

Class for creating cryptographic hmac content.

Returned by `crypto.createHmac`.

### hmac.update(data)

Update the hmac content with the given `data`.
This can be called many times with new data as it is streamed.

### hmac.digest([encoding])

Calculates the digest of all of the passed data to the hmac.
The `encoding` can be `'hex'`, `'binary'` or `'base64'`.
Defaults to `'binary'`.

Note: `hmac` object can not be used after `digest()` method been called.


## crypto.createCipher(algorithm, password)

Creates and returns a cipher object, with the given algorithm and password.

`algorithm` is dependent on OpenSSL, examples are `'aes192'`, etc.
On recent releases, `openssl list-cipher-algorithms` will display the
available cipher algorithms.
`password` is used to derive key and IV, which must be `'binary'` encoded
string (See the [Buffer section](buffer.html) for more information).

## crypto.createCipheriv(algorithm, key, iv)

Creates and returns a cipher object, with the given algorithm, key and iv.

`algorithm` is the same as the `createCipher()`. `key` is a raw key used in
algorithm. `iv` is an Initialization vector. `key` and `iv` must be `'binary'`
encoded string (See the [Buffer section](buffer.html) for more information).

## Class: Cipher

Class for encrypting data.

Returned by `crypto.createCipher` and `crypto.createCipheriv`.

### cipher.update(data, [input_encoding], [output_encoding])

Updates the cipher with `data`, the encoding of which is given in
`input_encoding` and can be `'utf8'`, `'ascii'` or `'binary'`.
Defaults to `'binary'`.

The `output_encoding` specifies the output format of the enciphered data,
and can be `'binary'`, `'base64'` or `'hex'`. Defaults to `'binary'`.

Returns the enciphered contents, and can be called many times with new data as it is streamed.

### cipher.final([output_encoding])

Returns any remaining enciphered contents, with `output_encoding` being one of:
`'binary'`, `'base64'` or `'hex'`. Defaults to `'binary'`.

Note: `cipher` object can not be used after `final()` method been called.


## crypto.createDecipher(algorithm, password)

Creates and returns a decipher object, with the given algorithm and key.
This is the mirror of the [createCipher()](#crypto.createCipher) above.

## crypto.createDecipheriv(algorithm, key, iv)

Creates and returns a decipher object, with the given algorithm, key and iv.
This is the mirror of the [createCipheriv()](#crypto.createCipheriv) above.

## Class: Decipher

Class for decrypting data.

Returned by `crypto.createDecipher` and `crypto.createDecipheriv`.

### decipher.update(data, [input_encoding], [output_encoding])

Updates the decipher with `data`, which is encoded in `'binary'`, `'base64'`
or `'hex'`. Defaults to `'binary'`.

The `output_decoding` specifies in what format to return the deciphered
plaintext: `'binary'`, `'ascii'` or `'utf8'`. Defaults to `'binary'`.

### decipher.final([output_encoding])

Returns any remaining plaintext which is deciphered,
with `output_encoding` being one of: `'binary'`, `'ascii'` or `'utf8'`.
Defaults to `'binary'`.

Note: `decipher` object can not be used after `final()` method been called.


## crypto.createSign(algorithm)

Creates and returns a signing object, with the given algorithm.
On recent OpenSSL releases, `openssl list-public-key-algorithms` will display
the available signing algorithms. Examples are `'RSA-SHA256'`.

## Class: Signer

Class for generating signatures.

Returned by `crypto.createSign`.

### signer.update(data)

Updates the signer object with data.
This can be called many times with new data as it is streamed.

### signer.sign(private_key, [output_format])

Calculates the signature on all the updated data passed through the signer.
`private_key` is a string containing the PEM encoded private key for signing.

Returns the signature in `output_format` which can be `'binary'`, `'hex'` or
`'base64'`. Defaults to `'binary'`.

Note: `signer` object can not be used after `sign()` method been called.

## crypto.createVerify(algorithm)

Creates and returns a verification object, with the given algorithm.
This is the mirror of the signing object above.

## Class: Verify

Class for verifying signatures.

Returned by `crypto.createVerify`.

### verifier.update(data)

Updates the verifier object with data.
This can be called many times with new data as it is streamed.

### verifier.verify(object, signature, [signature_format])

Verifies the signed data by using the `object` and `signature`. `object` is  a
string containing a PEM encoded object, which can be one of RSA public key,
DSA public key, or X.509 certificate. `signature` is the previously calculated
signature for the data, in the `signature_format` which can be `'binary'`,
`'hex'` or `'base64'`. Defaults to `'binary'`.

Returns true or false depending on the validity of the signature for the data and public key.

Note: `verifier` object can not be used after `verify()` method been called.

## crypto.createDiffieHellman(prime_length)

Creates a Diffie-Hellman key exchange object and generates a prime of the
given bit length. The generator used is `2`.

## crypto.createDiffieHellman(prime, [encoding])

Creates a Diffie-Hellman key exchange object using the supplied prime. The
generator used is `2`. Encoding can be `'binary'`, `'hex'`, or `'base64'`.
Defaults to `'binary'`.

## Class: DiffieHellman

The class for creating Diffie-Hellman key exchanges.

Returned by `crypto.createDiffieHellman`.

### diffieHellman.generateKeys([encoding])

Generates private and public Diffie-Hellman key values, and returns the
public key in the specified encoding. This key should be transferred to the
other party. Encoding can be `'binary'`, `'hex'`, or `'base64'`.
Defaults to `'binary'`.

### diffieHellman.computeSecret(other_public_key, [input_encoding], [output_encoding])

Computes the shared secret using `other_public_key` as the other party's
public key and returns the computed shared secret. Supplied key is
interpreted using specified `input_encoding`, and secret is encoded using
specified `output_encoding`. Encodings can be `'binary'`, `'hex'`, or
`'base64'`. The input encoding defaults to `'binary'`.
If no output encoding is given, the input encoding is used as output encoding.

### diffieHellman.getPrime([encoding])

Returns the Diffie-Hellman prime in the specified encoding, which can be
`'binary'`, `'hex'`, or `'base64'`. Defaults to `'binary'`.

### diffieHellman.getGenerator([encoding])

Returns the Diffie-Hellman prime in the specified encoding, which can be
`'binary'`, `'hex'`, or `'base64'`. Defaults to `'binary'`.

### diffieHellman.getPublicKey([encoding])

Returns the Diffie-Hellman public key in the specified encoding, which can
be `'binary'`, `'hex'`, or `'base64'`. Defaults to `'binary'`.

### diffieHellman.getPrivateKey([encoding])

Returns the Diffie-Hellman private key in the specified encoding, which can
be `'binary'`, `'hex'`, or `'base64'`. Defaults to `'binary'`.

### diffieHellman.setPublicKey(public_key, [encoding])

Sets the Diffie-Hellman public key. Key encoding can be `'binary'`, `'hex'`,
or `'base64'`. Defaults to `'binary'`.

### diffieHellman.setPrivateKey(public_key, [encoding])

Sets the Diffie-Hellman private key. Key encoding can be `'binary'`, `'hex'`,
or `'base64'`. Defaults to `'binary'`.

## crypto.pbkdf2(password, salt, iterations, keylen, callback)

Asynchronous PBKDF2 applies pseudorandom function HMAC-SHA1 to derive
a key of given length from the given password, salt and iterations.
The callback gets two arguments `(err, derivedKey)`.

## crypto.randomBytes(size, [callback])

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
    }
