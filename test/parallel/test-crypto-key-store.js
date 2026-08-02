'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const { hasOpenSSL } = require('../common/crypto');
if (!hasOpenSSL(3))
  common.skip('requires OpenSSL 3.x');

// Verifies that crypto.createPrivateKey() can pass a WHATWG URL (here a file:
// URI) to an OpenSSL STORE loader, and that the resulting KeyObject works for
// signing and verification.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const {
  createPublicKey,
  createPrivateKey,
  createVerify,
  decapsulate,
  diffieHellman,
  encapsulate,
  generateKeyPairSync,
  privateDecrypt,
  privateEncrypt,
  publicDecrypt,
  publicEncrypt,
  sign,
  verify,
} = require('crypto');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const data = Buffer.from('hello store');

{
  const { privateKey, publicKey } = generateKeyPairSync('ed25519');
  const file = path.join(tmpdir.path, 'priv.pem');
  fs.writeFileSync(file, privateKey.export({ format: 'pem', type: 'pkcs8' }));
  const url = pathToFileURL(file);

  const pk = createPrivateKey(url);
  assert.strictEqual(pk.type, 'private');
  assert.strictEqual(pk.asymmetricKeyType, 'ed25519');

  const sig = sign(null, data, pk);
  assert.strictEqual(verify(null, data, publicKey, sig), true);

  const pkWithProperties = createPrivateKey({ key: url, properties: '' });
  assert.strictEqual(pkWithProperties.type, 'private');
  assert.strictEqual(pkWithProperties.asymmetricKeyType, 'ed25519');
  assert.strictEqual(
    verify(null, data, publicKey, sign(null, data, pkWithProperties)),
    true);

  // Passing the URL inline to sign() behaves like createPrivateKey().
  assert.strictEqual(verify(null, data, publicKey, sign(null, data, url)), true);

  assert.throws(() => createPrivateKey({ key: url, properties: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

{
  const { privateKey, publicKey } = generateKeyPairSync('rsa', {
    modulusLength: 2048,
  });
  const file = path.join(tmpdir.path, 'rsa.pem');
  fs.writeFileSync(file, privateKey.export({ format: 'pem', type: 'pkcs8' }));
  const url = pathToFileURL(file);
  const plaintext = Buffer.from('hello rsa store');

  const ciphertext = publicEncrypt(publicKey, plaintext);
  assert.deepStrictEqual(privateDecrypt(url, ciphertext), plaintext);
  assert.deepStrictEqual(privateDecrypt({ key: url }, ciphertext), plaintext);

  const encrypted = privateEncrypt(url, plaintext);
  assert.deepStrictEqual(publicDecrypt(publicKey, encrypted), plaintext);

  const encryptedFromObject = privateEncrypt({ key: url }, plaintext);
  assert.deepStrictEqual(publicDecrypt(publicKey, encryptedFromObject),
                         plaintext);
}

{
  const alice = generateKeyPairSync('x25519');
  const bob = generateKeyPairSync('x25519');
  const file = path.join(tmpdir.path, 'x25519.pem');
  fs.writeFileSync(file, alice.privateKey.export({
    format: 'pem',
    type: 'pkcs8',
  }));
  const url = pathToFileURL(file);

  const expected = diffieHellman({
    privateKey: alice.privateKey,
    publicKey: bob.publicKey,
  });
  assert.deepStrictEqual(
    diffieHellman({ privateKey: url, publicKey: bob.publicKey }),
    expected);

  if (hasOpenSSL(3, 2)) {
    const { sharedKey, ciphertext } = encapsulate(alice.publicKey);
    assert.deepStrictEqual(decapsulate(url, ciphertext), sharedKey);
  }
}

{
  // Encrypted PKCS#8 with passphrase via { key: url, passphrase }.
  const { privateKey, publicKey } = generateKeyPairSync('ed25519');
  const file = path.join(tmpdir.path, 'enc.pem');
  fs.writeFileSync(file, privateKey.export({
    format: 'pem', type: 'pkcs8', cipher: 'aes-256-cbc', passphrase: 'pw',
  }));
  const url = pathToFileURL(file);

  const sig = sign(null, data, { key: url, passphrase: Buffer.from('pw') });
  assert.strictEqual(verify(null, data, publicKey, sig), true);

  assert.throws(() => createPrivateKey(url), {
    code: 'ERR_MISSING_PASSPHRASE',
  });

  assert.throws(() => createPrivateKey({ key: url, passphrase: 'bad' }),
                common.expectsError({
                  name: 'Error',
                  code: /^ERR_OSSL_/,
                }));
}

{
  // A URL is only accepted in private-key contexts.
  const url = pathToFileURL(path.join(tmpdir.path, 'priv.pem'));
  assert.throws(() => createPublicKey(url), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => createPublicKey({ key: url }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => publicEncrypt(url, data), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => publicEncrypt({ key: url }, data), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => publicDecrypt(url, Buffer.alloc(0)), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => verify(null, data, url, Buffer.alloc(0)), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => verify(null, data, { key: url }, Buffer.alloc(0)), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  const verifier = createVerify('sha256');
  verifier.update(data);
  assert.throws(() => verifier.verify(url, Buffer.alloc(0)), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => encapsulate(url), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => encapsulate({ key: url }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => diffieHellman({
    privateKey: createPrivateKey(url),
    publicKey: url,
  }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => createPrivateKey(1), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /URL/,
  });
}

{
  // A readable URI that holds no private key is distinguishable from a URI
  // that could not be opened at all.
  const file = path.join(tmpdir.path, 'nope.pem');
  fs.writeFileSync(file, 'not a key');
  assert.throws(() => createPrivateKey(pathToFileURL(file)), {
    code: 'ERR_CRYPTO_OPERATION_FAILED',
  });

  // OpenSSL has no reason string for system library errors, so error.code
  // cannot be derived for these. Which entry becomes the message is not
  // portable either: some builds leave a generic STORE `unsupported` on top of
  // the error the loader itself raised. The reason is reported either way.
  assert.throws(
    () => createPrivateKey(pathToFileURL(path.join(tmpdir.path, 'missing.pem'))),
    (err) => {
      assert.match([err.message, ...err.opensslErrorStack ?? []].join('\n'),
                   /No such file or directory/);
      return true;
    });
}

{
  // Failures report the loader that actually handled the URI, not the `file`
  // loader that OpenSSL always probes first for URIs without an authority.
  assert.throws(() => createPrivateKey(new URL('pkcs11:object=nope')), {
    code: 'ERR_OSSL_OSSL_STORE_UNSUPPORTED',
  });
}

{
  // Only a genuine URL selects the STORE loader. A plain object that merely
  // exposes `href` and `protocol` must not be treated as one, otherwise an
  // attacker-supplied JWK could redirect the load to a URI of their choosing.
  const file = path.join(tmpdir.path, 'priv.pem');
  const spoofed = {
    kty: 'EC',
    crv: 'P-256',
    x: 'a',
    y: 'b',
    d: 'c',
    href: pathToFileURL(file).href,
    protocol: 'file:',
  };
  assert.throws(() => createPrivateKey({ key: spoofed, format: 'jwk' }), {
    code: 'ERR_CRYPTO_INVALID_JWK',
  });
  assert.throws(() => createPrivateKey(spoofed), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(
    () => sign(null, data, { key: spoofed, format: 'jwk' }),
    { code: 'ERR_CRYPTO_INVALID_JWK' });
}

{
  // Read the URI from the URL's private state. Public accessors on a branded
  // subclass must not redirect a previously created URL to a different key.
  const trustedHref = pathToFileURL(
    path.join(tmpdir.path, 'priv.pem')).href;
  const redirectHref = pathToFileURL(
    path.join(tmpdir.path, 'rsa.pem')).href;
  class RedirectingURL extends URL {
    get href() { return redirectHref; }
  }
  const subclassURL = new RedirectingURL(trustedHref);
  assert.strictEqual(
    URL.prototype.toString.call(subclassURL), trustedHref);
  assert.strictEqual(
    createPrivateKey(subclassURL).asymmetricKeyType, 'ed25519');

  // The same applies when the accessor is replaced on URL.prototype.
  const prototypeURL = new URL(trustedHref);
  const hrefDescriptor = Object.getOwnPropertyDescriptor(URL.prototype, 'href');
  try {
    Object.defineProperty(URL.prototype, 'href', {
      ...hrefDescriptor,
      get() { return redirectHref; },
    });
    assert.strictEqual(
      URL.prototype.toString.call(prototypeURL), trustedHref);
    assert.strictEqual(
      createPrivateKey(prototypeURL).asymmetricKeyType, 'ed25519');
  } finally {
    Object.defineProperty(URL.prototype, 'href', hrefDescriptor);
  }

  // Replacing `protocol` must not turn an opaque STORE URI into a file path.
  const filePathname = pathToFileURL(
    path.join(tmpdir.path, 'priv.pem')).pathname;
  const opaqueURL = new URL(`pkcs11:${filePathname}`);
  const protocolDescriptor = Object.getOwnPropertyDescriptor(
    URL.prototype, 'protocol');
  try {
    Object.defineProperty(URL.prototype, 'protocol', {
      ...protocolDescriptor,
      get() { return 'file:'; },
    });
    assert.throws(() => createPrivateKey(opaqueURL), {
      code: 'ERR_OSSL_OSSL_STORE_UNSUPPORTED',
    });
  } finally {
    Object.defineProperty(URL.prototype, 'protocol', protocolDescriptor);
  }
}

{
  // The URI is handed to OpenSSL as a NUL-terminated C string, so an embedded
  // NUL must be rejected rather than silently truncating the path.
  const file = path.join(tmpdir.path, 'priv.pem');
  assert.throws(
    () => createPrivateKey(new URL(`${pathToFileURL(file).href}%00.txt`)), {
      code: 'ERR_INVALID_ARG_VALUE',
    });

  // Same for the property query.
  assert.throws(() => createPrivateKey({
    key: pathToFileURL(file),
    properties: `provider=def${'\u0000'}ault`,
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}
