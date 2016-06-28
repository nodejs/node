TweetNaCl.js Changelog
======================


v0.13.2
-------

* Fixed undefined variable bug in fast version of Poly1305. No worries, this
  bug was *never* triggered.

* Specified CC0 public domain dedication.

* Updated development dependencies.


v0.13.1
-------

* Exclude `crypto` and `buffer` modules from browserify builds.


v0.13.0
-------

* Made `nacl-fast` the default version in NPM package. Now
  `require("tweetnacl")` will use fast version; to get the original version,
  use `require("tweetnacl/nacl.js")`.

* Cleanup temporary array after generating random bytes.


v0.12.2
-------

* Improved performance of curve operations, making `nacl.scalarMult`, `nacl.box`,
  `nacl.sign` and related functions up to 3x faster in `nacl-fast` version.


v0.12.1
-------

* Significantly improved performance of Salsa20 (~1.5x faster) and
  Poly1305 (~3.5x faster) in `nacl-fast` version.


v0.12.0
-------

* Instead of using the given secret key directly, TweetNaCl.js now copies it to
  a new array in `nacl.box.keyPair.fromSecretKey` and
  `nacl.sign.keyPair.fromSecretKey`.


v0.11.2
-------

* Added new constant: `nacl.sign.seedLength`.


v0.11.1
-------

* Even faster hash for both short and long inputs (in `nacl-fast`).


v0.11.0
-------

* Implement `nacl.sign.keyPair.fromSeed` to enable creation of sign key pairs
  deterministically from a 32-byte seed. (It behaves like
  [libsodium's](http://doc.libsodium.org/public-key_cryptography/public-key_signatures.html)
  `crypto_sign_seed_keypair`: the seed becomes a secret part of the secret key.)

* Fast version now has an improved hash implementation that is 2x-5x faster.

* Fixed benchmarks, which may have produced incorrect measurements.


v0.10.1
-------

* Exported undocumented `nacl.lowlevel.crypto_core_hsalsa20`.


v0.10.0
-------

* **Signature API breaking change!** `nacl.sign` and `nacl.sign.open` now deal
 with signed messages, and new `nacl.sign.detached` and
 `nacl.sign.detached.verify` are available.
 
 Previously, `nacl.sign` returned a signature, and `nacl.sign.open` accepted a
 message and "detached" signature. This was unlike NaCl's API, which dealt with
 signed messages (concatenation of signature and message).
 
 The new API is:

      nacl.sign(message, secretKey) -> signedMessage
      nacl.sign.open(signedMessage, publicKey) -> message | null

 Since detached signatures are common, two new API functions were introduced:
 
      nacl.sign.detached(message, secretKey) -> signature
      nacl.sign.detached.verify(message, signature, publicKey) -> true | false

 (Note that it's `verify`, not `open`, and it returns a boolean value, unlike
 `open`, which returns an "unsigned" message.)

* NPM package now comes without `test` directory to keep it small.


v0.9.2
------

* Improved documentation.
* Fast version: increased theoretical message size limit from 2^32-1 to 2^52
  bytes in Poly1305 (and thus, secretbox and box). However this has no impact
  in practice since JavaScript arrays or ArrayBuffers are limited to 32-bit
  indexes, and most implementations won't allocate more than a gigabyte or so.
  (Obviously, there are no tests for the correctness of implementation.) Also,
  it's not recommended to use messages that large without splitting them into
  smaller packets anyway.


v0.9.1
------

* Initial release
