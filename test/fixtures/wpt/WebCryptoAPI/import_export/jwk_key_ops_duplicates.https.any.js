// META: title=WebCryptoAPI: importKey rejects a JWK with duplicate key_ops
// META: global=window,worker

'use strict';

// RFC 7517 section 4.3: "Duplicate key operation values MUST NOT be present".
// WebCrypto rejects a key_ops member that is invalid per JWK with DataError.
promise_test(async t => {
  const jwk = {
    kty: 'oct',
    k: 'AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8',
    alg: 'HS256',
    key_ops: ['sign', 'verify', 'verify'],
  };
  await promise_rejects_dom(t, 'DataError',
    crypto.subtle.importKey('jwk', jwk, {name: 'HMAC', hash: 'SHA-256'},
                            true, ['sign']));
}, 'importKey rejects a JWK whose key_ops has a duplicate');
