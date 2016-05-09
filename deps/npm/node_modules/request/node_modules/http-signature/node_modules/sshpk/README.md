sshpk
=========

Parse, convert, fingerprint and use SSH keys (both public and private) in pure
node -- no `ssh-keygen` or other external dependencies.

Supports RSA, DSA, ECDSA (nistp-\*) and ED25519 key types, in PEM (PKCS#1, 
PKCS#8) and OpenSSH formats.

This library has been extracted from
[`node-http-signature`](https://github.com/joyent/node-http-signature)
(work by [Mark Cavage](https://github.com/mcavage) and
[Dave Eddy](https://github.com/bahamas10)) and
[`node-ssh-fingerprint`](https://github.com/bahamas10/node-ssh-fingerprint)
(work by Dave Eddy), with additions (including ECDSA support) by
[Alex Wilson](https://github.com/arekinath).

Install
-------

```
npm install sshpk
```

Examples
--------

```js
var sshpk = require('sshpk');

var fs = require('fs');

/* Read in an OpenSSH-format public key */
var keyPub = fs.readFileSync('id_rsa.pub');
var key = sshpk.parseKey(keyPub, 'ssh');

/* Get metadata about the key */
console.log('type => %s', key.type);
console.log('size => %d bits', key.size);
console.log('comment => %s', key.comment);

/* Compute key fingerprints, in new OpenSSH (>6.7) format, and old MD5 */
console.log('fingerprint => %s', key.fingerprint().toString());
console.log('old-style fingerprint => %s', key.fingerprint('md5').toString());
```

Example output:

```
type => rsa
size => 2048 bits
comment => foo@foo.com
fingerprint => SHA256:PYC9kPVC6J873CSIbfp0LwYeczP/W4ffObNCuDJ1u5w
old-style fingerprint => a0:c8:ad:6c:32:9a:32:fa:59:cc:a9:8c:0a:0d:6e:bd
```

More examples: converting between formats:

```js
/* Read in a PEM public key */
var keyPem = fs.readFileSync('id_rsa.pem');
var key = sshpk.parseKey(keyPem, 'pem');

/* Convert to PEM PKCS#8 public key format */
var pemBuf = key.toBuffer('pkcs8');

/* Convert to SSH public key format (and return as a string) */
var sshKey = key.toString('ssh');
```

Signing and verifying:

```js
/* Read in an OpenSSH/PEM *private* key */
var keyPriv = fs.readFileSync('id_ecdsa');
var key = sshpk.parsePrivateKey(keyPriv, 'pem');

var data = 'some data';

/* Sign some data with the key */
var s = key.createSign('sha1');
s.update(data);
var signature = s.sign();

/* Now load the public key (could also use just key.toPublic()) */
var keyPub = fs.readFileSync('id_ecdsa.pub');
key = sshpk.parseKey(keyPub, 'ssh');

/* Make a crypto.Verifier with this key */
var v = key.createVerify('sha1');
v.update(data);
var valid = v.verify(signature);
/* => true! */
```

Matching fingerprints with keys:

```js
var fp = sshpk.parseFingerprint('SHA256:PYC9kPVC6J873CSIbfp0LwYeczP/W4ffObNCuDJ1u5w');

var keys = [sshpk.parseKey(...), sshpk.parseKey(...), ...];

keys.forEach(function (key) {
	if (fp.matches(key))
		console.log('found it!');
});
```

Usage
-----

## Public keys

### `parseKey(data[, format = 'auto'[, name]])`

Parses a key from a given data format and returns a new `Key` object.

Parameters

- `data` -- Either a Buffer or String, containing the key
- `format` -- String name of format to use, valid options are:
  - `auto`: choose automatically from all below
  - `pem`: supports both PKCS#1 and PKCS#8
  - `ssh`: standard OpenSSH format,
  - `pkcs1`, `pkcs8`: variants of `pem`
  - `rfc4253`: raw OpenSSH wire format
  - `openssh`: new post-OpenSSH 6.5 internal format, produced by 
               `ssh-keygen -o`
- `name` -- Optional name for the key being parsed (eg. the filename that
            was opened). Used to generate Error messages

### `Key.isKey(obj)`

Returns `true` if the given object is a valid `Key` object created by a version
of `sshpk` compatible with this one.

Parameters

- `obj` -- Object to identify

### `Key#type`

String, the type of key. Valid options are `rsa`, `dsa`, `ecdsa`.

### `Key#size`

Integer, "size" of the key in bits. For RSA/DSA this is the size of the modulus;
for ECDSA this is the bit size of the curve in use.

### `Key#comment`

Optional string, a key comment used by some formats (eg the `ssh` format).

### `Key#curve`

Only present if `this.type === 'ecdsa'`, string containing the name of the
named curve used with this key. Possible values include `nistp256`, `nistp384`
and `nistp521`.

### `Key#toBuffer([format = 'ssh'])`

Convert the key into a given data format and return the serialized key as
a Buffer.

Parameters

- `format` -- String name of format to use, for valid options see `parseKey()`

### `Key#toString([format = 'ssh])`

Same as `this.toBuffer(format).toString()`.

### `Key#fingerprint([algorithm = 'sha256'])`

Creates a new `Fingerprint` object representing this Key's fingerprint.

Parameters

- `algorithm` -- String name of hash algorithm to use, valid options are `md5`,
                 `sha1`, `sha256`, `sha384`, `sha512`

### `Key#createVerify([hashAlgorithm])`

Creates a `crypto.Verifier` specialized to use this Key (and the correct public
key algorithm to match it). The returned Verifier has the same API as a regular
one, except that the `verify()` function takes only the target signature as an
argument.

Parameters

- `hashAlgorithm` -- optional String name of hash algorithm to use, any
                     supported by OpenSSL are valid, usually including
                     `sha1`, `sha256`.

`v.verify(signature[, format])` Parameters

- `signature` -- either a Signature object, or a Buffer or String
- `format` -- optional String, name of format to interpret given String with.
              Not valid if `signature` is a Signature or Buffer.

### `Key#createDiffieHellman()`
### `Key#createDH()`

Creates a Diffie-Hellman key exchange object initialized with this key and all
necessary parameters. This has the same API as a `crypto.DiffieHellman`
instance, except that functions take `Key` and `PrivateKey` objects as
arguments, and return them where indicated for.

This is only valid for keys belonging to a cryptosystem that supports DHE
or a close analogue (i.e. `dsa`, `ecdsa` and `curve25519` keys). An attempt
to call this function on other keys will yield an `Error`.

## Private keys

### `parsePrivateKey(data[, format = 'auto'[, name]])`

Parses a private key from a given data format and returns a new
`PrivateKey` object.

Parameters

- `data` -- Either a Buffer or String, containing the key
- `format` -- String name of format to use, valid options are:
  - `auto`: choose automatically from all below
  - `pem`: supports both PKCS#1 and PKCS#8
  - `ssh`, `openssh`: new post-OpenSSH 6.5 internal format, produced by 
                      `ssh-keygen -o`
  - `pkcs1`, `pkcs8`: variants of `pem`
  - `rfc4253`: raw OpenSSH wire format
- `name` -- Optional name for the key being parsed (eg. the filename that
            was opened). Used to generate Error messages

### `PrivateKey.isPrivateKey(obj)`

Returns `true` if the given object is a valid `PrivateKey` object created by a
version of `sshpk` compatible with this one.

Parameters

- `obj` -- Object to identify

### `PrivateKey#type`

String, the type of key. Valid options are `rsa`, `dsa`, `ecdsa`.

### `PrivateKey#size`

Integer, "size" of the key in bits. For RSA/DSA this is the size of the modulus;
for ECDSA this is the bit size of the curve in use.

### `PrivateKey#curve`

Only present if `this.type === 'ecdsa'`, string containing the name of the
named curve used with this key. Possible values include `nistp256`, `nistp384`
and `nistp521`.

### `PrivateKey#toBuffer([format = 'pkcs1'])`

Convert the key into a given data format and return the serialized key as
a Buffer.

Parameters

- `format` -- String name of format to use, valid options are listed under 
              `parsePrivateKey`. Note that ED25519 keys default to `openssh`
              format instead (as they have no `pkcs1` representation).

### `PrivateKey#toString([format = 'pkcs1'])`

Same as `this.toBuffer(format).toString()`.

### `PrivateKey#toPublic()`

Extract just the public part of this private key, and return it as a `Key`
object.

### `PrivateKey#fingerprint([algorithm = 'sha256'])`

Same as `this.toPublic().fingerprint()`.

### `PrivateKey#createVerify([hashAlgorithm])`

Same as `this.toPublic().createVerify()`.

### `PrivateKey#createSign([hashAlgorithm])`

Creates a `crypto.Sign` specialized to use this PrivateKey (and the correct
key algorithm to match it). The returned Signer has the same API as a regular
one, except that the `sign()` function takes no arguments, and returns a
`Signature` object.

Parameters

- `hashAlgorithm` -- optional String name of hash algorithm to use, any
                     supported by OpenSSL are valid, usually including
                     `sha1`, `sha256`.

`v.sign()` Parameters

- none

### `PrivateKey#derive(newType)`

Derives a related key of type `newType` from this key. Currently this is
only supported to change between `ed25519` and `curve25519` keys which are
stored with the same private key (but usually distinct public keys in order
to avoid degenerate keys that lead to a weak Diffie-Hellman exchange).

Parameters

- `newType` -- String, type of key to derive, either `ed25519` or `curve25519`

## Fingerprints

### `parseFingerprint(fingerprint[, algorithms])`

Pre-parses a fingerprint, creating a `Fingerprint` object that can be used to
quickly locate a key by using the `Fingerprint#matches` function.

Parameters

- `fingerprint` -- String, the fingerprint value, in any supported format
- `algorithms` -- Optional list of strings, names of hash algorithms to limit
                  support to. If `fingerprint` uses a hash algorithm not on
                  this list, throws `InvalidAlgorithmError`.

### `Fingerprint.isFingerprint(obj)`

Returns `true` if the given object is a valid `Fingerprint` object created by a
version of `sshpk` compatible with this one.

Parameters

- `obj` -- Object to identify

### `Fingerprint#toString([format])`

Returns a fingerprint as a string, in the given format.

Parameters

- `format` -- Optional String, format to use, valid options are `hex` and
              `base64`. If this `Fingerprint` uses the `md5` algorithm, the
              default format is `hex`. Otherwise, the default is `base64`.

### `Fingerprint#matches(key)`

Verifies whether or not this `Fingerprint` matches a given `Key`. This function
uses double-hashing to avoid leaking timing information. Returns a boolean.

Parameters

- `key` -- a `Key` object, the key to match this fingerprint against

## Signatures

### `parseSignature(signature, algorithm, format)`

Parses a signature in a given format, creating a `Signature` object. Useful
for converting between the SSH and ASN.1 (PKCS/OpenSSL) signature formats, and
also returned as output from `PrivateKey#createSign().sign()`.

A Signature object can also be passed to a verifier produced by
`Key#createVerify()` and it will automatically be converted internally into the
correct format for verification.

Parameters

- `signature` -- a Buffer (binary) or String (base64), data of the actual
                 signature in the given format
- `algorithm` -- a String, name of the algorithm to be used, possible values
                 are `rsa`, `dsa`, `ecdsa`
- `format` -- a String, either `asn1` or `ssh`

### `Signature.isSignature(obj)`

Returns `true` if the given object is a valid `Signature` object created by a
version of `sshpk` compatible with this one.

Parameters

- `obj` -- Object to identify

### `Signature#toBuffer([format = 'asn1'])`

Converts a Signature to the given format and returns it as a Buffer.

Parameters

- `format` -- a String, either `asn1` or `ssh`

### `Signature#toString([format = 'asn1'])`

Same as `this.toBuffer(format).toString('base64')`.

Errors
------

### `InvalidAlgorithmError`

The specified algorithm is not valid, either because it is not supported, or
because it was not included on a list of allowed algorithms.

Thrown by `Fingerprint.parse`, `Key#fingerprint`.

Properties

- `algorithm` -- the algorithm that could not be validated

### `FingerprintFormatError`

The fingerprint string given could not be parsed as a supported fingerprint
format, or the specified fingerprint format is invalid.

Thrown by `Fingerprint.parse`, `Fingerprint#toString`.

Properties

- `fingerprint` -- if caused by a fingerprint, the string value given
- `format` -- if caused by an invalid format specification, the string value given

### `KeyParseError`

The key data given could not be parsed as a valid key.

Properties

- `keyName` -- `name` that was given to `Key#parse`
- `format` -- the `format` that was trying to parse the key
- `innerErr` -- the inner Error thrown by the format parser

Friends of sshpk
----------------

 * [`sshpk-agent`](https://github.com/arekinath/node-sshpk-agent) is a library
   for speaking the `ssh-agent` protocol from node.js, which uses `sshpk`
