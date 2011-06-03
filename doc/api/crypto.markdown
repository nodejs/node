## Crypto

Use `require('crypto')` to access this module.

The crypto module requires OpenSSL to be available on the underlying platform.
It offers a way of encapsulating secure credentials to be used as part
of a secure HTTPS net or http connection.

It also offers a set of wrappers for OpenSSL's hash, hmac, cipher, decipher, sign and verify methods.

### crypto.createCredentials(details)

Creates a credentials object, with the optional details being a dictionary with keys:

* `key` : a string holding the PEM encoded private key
* `cert` : a string holding the PEM encoded certificate
* `ca` : either a string or list of strings of PEM encoded CA certificates to trust.

If no 'ca' details are given, then node.js will use the default publicly trusted list of CAs as given in
<http://mxr.mozilla.org/mozilla/source/security/nss/lib/ckfw/builtins/certdata.txt>.


### crypto.createHash(algorithm)

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

### hash.update(data)

Updates the hash content with the given `data`.
This can be called many times with new data as it is streamed.

### hash.digest(encoding='binary')

Calculates the digest of all of the passed data to be hashed.
The `encoding` can be `'hex'`, `'binary'` or `'base64'`.


### crypto.createHmac(algorithm, key)

Creates and returns a hmac object, a cryptographic hmac with the given algorithm and key.

`algorithm` is dependent on the available algorithms supported by OpenSSL - see createHash above.
`key` is the hmac key to be used.

### hmac.update(data)

Update the hmac content with the given `data`.
This can be called many times with new data as it is streamed.

### hmac.digest(encoding='binary')

Calculates the digest of all of the passed data to the hmac.
The `encoding` can be `'hex'`, `'binary'` or `'base64'`.


### crypto.createCipher(algorithm, key)

Creates and returns a cipher object, with the given algorithm and key.

`algorithm` is dependent on OpenSSL, examples are `'aes192'`, etc.
On recent releases, `openssl list-cipher-algorithms` will display the available cipher algorithms.

### cipher.update(data, input_encoding='binary', output_encoding='binary')

Updates the cipher with `data`, the encoding of which is given in `input_encoding`
and can be `'utf8'`, `'ascii'` or `'binary'`. The `output_encoding` specifies
the output format of the enciphered data, and can be `'binary'`, `'base64'` or `'hex'`.

Returns the enciphered contents, and can be called many times with new data as it is streamed.

### cipher.final(output_encoding='binary')

Returns any remaining enciphered contents, with `output_encoding` being one of: `'binary'`, `'base64'` or `'hex'`.

### crypto.createDecipher(algorithm, key)

Creates and returns a decipher object, with the given algorithm and key.
This is the mirror of the cipher object above.

### decipher.update(data, input_encoding='binary', output_encoding='binary')

Updates the decipher with `data`, which is encoded in `'binary'`, `'base64'` or `'hex'`.
The `output_decoding` specifies in what format to return the deciphered plaintext: `'binary'`, `'ascii'` or `'utf8'`.

### decipher.final(output_encoding='binary')

Returns any remaining plaintext which is deciphered,
with `output_encoding` being one of: `'binary'`, `'ascii'` or `'utf8'`.


### crypto.createSign(algorithm)

Creates and returns a signing object, with the given algorithm.
On recent OpenSSL releases, `openssl list-public-key-algorithms` will display
the available signing algorithms. Examples are `'RSA-SHA256'`.

### signer.update(data)

Updates the signer object with data.
This can be called many times with new data as it is streamed.

### signer.sign(private_key, output_format='binary')

Calculates the signature on all the updated data passed through the signer.
`private_key` is a string containing the PEM encoded private key for signing.

Returns the signature in `output_format` which can be `'binary'`, `'hex'` or `'base64'`.

### crypto.createVerify(algorithm)

Creates and returns a verification object, with the given algorithm.
This is the mirror of the signing object above.

### verifier.update(data)

Updates the verifier object with data.
This can be called many times with new data as it is streamed.

### verifier.verify(cert, signature, signature_format='binary')

Verifies the signed data by using the `cert` which is a string containing
the PEM encoded certificate, and `signature`, which is the previously calculated
signature for the data, in the `signature_format` which can be `'binary'`, `'hex'` or `'base64'`.

Returns true or false depending on the validity of the signature for the data and public key.

### crypto.createDiffieHellman(prime_length)

Creates a Diffie-Hellman key exchange object and generates a prime of the
given bit length. The generator used is `2`.

### crypto.createDiffieHellman(prime, encoding='binary')

Creates a Diffie-Hellman key exchange object using the supplied prime. The
generator used is `2`. Encoding can be `'binary'`, `'hex'`, or `'base64'`.

### diffieHellman.generateKeys(encoding='binary')

Generates private and public Diffie-Hellman key values, and returns the
public key in the specified encoding. This key should be transferred to the
other party. Encoding can be `'binary'`, `'hex'`, or `'base64'`.

### diffieHellman.computeSecret(other_public_key, input_encoding='binary', output_encoding=input_encoding)

Computes the shared secret using `other_public_key` as the other party's
public key and returns the computed shared secret. Supplied key is
interpreted using specified `input_encoding`, and secret is encoded using
specified `output_encoding`. Encodings can be `'binary'`, `'hex'`, or
`'base64'`. If no output encoding is given, the input encoding is used as
output encoding.

### diffieHellman.getPrime(encoding='binary')

Returns the Diffie-Hellman prime in the specified encoding, which can be
`'binary'`, `'hex'`, or `'base64'`.

### diffieHellman.getGenerator(encoding='binary')

Returns the Diffie-Hellman prime in the specified encoding, which can be
`'binary'`, `'hex'`, or `'base64'`.

### diffieHellman.getPublicKey(encoding='binary')

Returns the Diffie-Hellman public key in the specified encoding, which can
be `'binary'`, `'hex'`, or `'base64'`.

### diffieHellman.getPrivateKey(encoding='binary')

Returns the Diffie-Hellman private key in the specified encoding, which can
be `'binary'`, `'hex'`, or `'base64'`.

### diffieHellman.setPublicKey(public_key, encoding='binary')

Sets the Diffie-Hellman public key. Key encoding can be `'binary'`, `'hex'`,
or `'base64'`.

### diffieHellman.setPrivateKey(public_key, encoding='binary')

Sets the Diffie-Hellman private key. Key encoding can be `'binary'`, `'hex'`, or `'base64'`.

