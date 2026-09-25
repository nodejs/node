'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');
const { hasOpenSSL3, hasFIPS } = require('../common/crypto');

const fips3 = hasFIPS(3);

if (fips3) {
  // The long-standing fixtures protect their contents with PKCS12KDF and
  // 3DES, neither of which the FIPS provider offers, so they cannot be opened
  // at all. Assert the recognizable error rather than skipping: this is the
  // same path the legacy.pfx case below exercises.
  for (const name of ['rsa_cert.pfx', 'agent1.pfx']) {
    assert.throws(
      () => crypto.parsePKCS12(fixtures.readKey(name), { passphrase: 'sample' }),
      { code: 'ERR_CRYPTO_UNSUPPORTED_OPERATION' });
  }

  // Reading the PBMAC1-protected FIPS fixtures needs OpenSSL 3.5 or newer, so
  // below that there is nothing further this test can check.
  if (!hasFIPS(3, 5)) return;
}

// The FIPS fixtures use PBMAC1/PBKDF2 and AES-256 in place of PKCS12KDF and
// 3DES. Their passphrase is eight characters because the OpenSSL 4 FIPS
// provider rejects shorter ones outright, which would mask the errors the
// cases below are actually about.
const keyAndCert = fips3 ?
  { name: 'agent1-fips.pfx', passphrase: 'password', additional: 1 } :
  { name: 'rsa_cert.pfx', passphrase: 'sample', additional: 0 };
const withCaCert = fips3 ?
  { name: 'agent1-fips.pfx', passphrase: 'password' } :
  { name: 'agent1.pfx', passphrase: 'sample' };
const ecKeyAndCert = fips3 ?
  { name: 'ec-fips.pfx', passphrase: 'password' } :
  { name: 'ec.pfx', passphrase: '' };
const certWithoutKey = fips3 ?
  { name: 'cert-without-key-fips.pfx', passphrase: 'password' } :
  { name: 'cert-without-key.pfx', passphrase: 'test' };

{
  // Round-trip: a bundle holding one key and its certificate.
  const bundle = fixtures.readKey(keyAndCert.name);
  const { privateKey, certificate, additionalCertificates } =
    crypto.parsePKCS12(bundle, { passphrase: keyAndCert.passphrase });

  assert.strictEqual(privateKey.type, 'private');
  assert.strictEqual(privateKey.asymmetricKeyType, 'rsa');
  assert.ok(certificate instanceof crypto.X509Certificate);
  assert.ok(Array.isArray(additionalCertificates));
  assert.strictEqual(additionalCertificates.length, keyAndCert.additional);

  // The parsed key and certificate must actually correspond.
  assert.strictEqual(certificate.checkPrivateKey(privateKey), true);

  // PEM export -- the primary reason callers want this API at all.
  const keyPem = privateKey.export({ type: 'pkcs8', format: 'pem' });
  assert.match(keyPem, /^-----BEGIN PRIVATE KEY-----/);
  assert.match(certificate.toString(), /^-----BEGIN CERTIFICATE-----/);

  // Round-trips back through createPrivateKey.
  const reimported = crypto.createPrivateKey(keyPem);
  assert.strictEqual(reimported.asymmetricKeyType, 'rsa');

  // Passphrase accepted as a buffer as well as a string.
  const { privateKey: key2 } = crypto.parsePKCS12(
    bundle, { passphrase: Buffer.from(keyAndCert.passphrase) });
  assert.strictEqual(key2.type, 'private');
}

{
  // A bundle carrying a second certificate alongside the end-entity one.
  const bundle = fixtures.readKey(withCaCert.name);
  const { privateKey, certificate, additionalCertificates } =
    crypto.parsePKCS12(bundle, { passphrase: withCaCert.passphrase });
  assert.strictEqual(privateKey.type, 'private');
  assert.ok(certificate instanceof crypto.X509Certificate);
  assert.strictEqual(additionalCertificates.length, 1);
  assert.ok(additionalCertificates[0] instanceof crypto.X509Certificate);
  assert.strictEqual(certificate.checkPrivateKey(privateKey), true);
}

{
  // EC key.
  const bundle = fixtures.readKey(ecKeyAndCert.name);
  const { privateKey, certificate } =
    crypto.parsePKCS12(bundle, { passphrase: ecKeyAndCert.passphrase });
  assert.strictEqual(privateKey.asymmetricKeyType, 'ec');
  assert.strictEqual(certificate.checkPrivateKey(privateKey), true);
}

if (!fips3) {
  // ec.pfx carries an empty passphrase, which the FIPS provider will not
  // accept at all, so this case has no FIPS counterpart. Omitting the
  // passphrase rather than passing it empty must behave identically: OpenSSL
  // tries both PKCS#12 password encodings, so either variant opens the file.
  const { privateKey } = crypto.parsePKCS12(fixtures.readKey('ec.pfx'));
  assert.strictEqual(privateKey.asymmetricKeyType, 'ec');
}

{
  // Wrong passphrase must be distinguishable from malformed input: OpenSSL
  // reports it as a MAC verification failure of its own, and BoringSSL
  // reports the same condition as an incorrect password. It is deliberately
  // longer than the FIPS minimum password length so that this checks what it
  // says it does under FIPS too.
  const code = process.features.openssl_is_boringssl ?
    'ERR_OSSL_INCORRECT_PASSWORD' : 'ERR_OSSL_PKCS12_MAC_VERIFY_FAILURE';
  const bundle = fixtures.readKey(keyAndCert.name);
  assert.throws(
    () => crypto.parsePKCS12(bundle, { passphrase: 'wrongpassphrase' }),
    { code });
}

{
  // Not a PKCS#12 structure at all. The error must be the same on either
  // crypto library: OpenSSL rejects this in d2i_PKCS12_bio(), while
  // BoringSSL's only copies the bytes and leaves every structural check to
  // PKCS12_parse(), so the binding has to recognize both.
  assert.throws(
    () => crypto.parsePKCS12(Buffer.from('not a bundle'), { passphrase: 'x' }),
    { code: 'ERR_CRYPTO_OPERATION_FAILED', message: /not a valid PKCS#12/ });
}

{
  // A bundle with no private key. PKCS12_parse() identifies the end-entity
  // certificate by its association with the key, so with no key there is
  // nothing to single out and everything arrives through
  // `additionalCertificates` -- including the end-entity certificate itself.
  const bundle = fixtures.readKey(certWithoutKey.name);
  const { privateKey, certificate, additionalCertificates } =
    crypto.parsePKCS12(bundle, { passphrase: certWithoutKey.passphrase });
  assert.strictEqual(privateKey, null);
  assert.strictEqual(certificate, null);
  assert.strictEqual(additionalCertificates.length, 1);
  assert.ok(additionalCertificates[0] instanceof crypto.X509Certificate);
}

if (hasOpenSSL3) {
  // Legacy algorithms (RC2-40-CBC) throw a recognizable, actionable error
  // rather than a bare OpenSSL string. Mirrors the behavior added for the
  // TLS path in https://github.com/nodejs/node/pull/54485.
  const bundle = fixtures.readKey('legacy.pfx');
  assert.throws(
    () => crypto.parsePKCS12(bundle, { passphrase: 'legacy' }),
    { code: 'ERR_CRYPTO_UNSUPPORTED_OPERATION' });
}

{
  // ArrayBuffer is a documented input type for both the bundle and the
  // passphrase, and must behave the same as a view over the same bytes.
  const buf = fixtures.readKey(keyAndCert.name);
  const bundleAb =
    buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);
  const passBuf = Buffer.from(keyAndCert.passphrase);
  const passAb =
    passBuf.buffer.slice(passBuf.byteOffset,
                         passBuf.byteOffset + passBuf.byteLength);

  const { privateKey, certificate } =
    crypto.parsePKCS12(bundleAb, { passphrase: passAb });
  assert.strictEqual(privateKey.type, 'private');
  assert.ok(certificate instanceof crypto.X509Certificate);
  assert.strictEqual(certificate.checkPrivateKey(privateKey), true);

  // DataView too.
  const { privateKey: key2 } = crypto.parsePKCS12(
    new DataView(bundleAb), { passphrase: keyAndCert.passphrase });
  assert.strictEqual(key2.type, 'private');
}

{
  // A passphrase reaches OpenSSL as a NUL-terminated C string, and
  // PKCS12_parse() takes no length alongside it. Without a check, everything
  // from an embedded NUL onwards is dropped and the bundle opens under the
  // truncated prefix -- a passphrase of '<pass>\0junk' would unlock a bundle
  // protected by '<pass>'. Reject the passphrase instead, whatever form it
  // arrives in.
  const bundle = fixtures.readKey(keyAndCert.name);
  const withNul = `${keyAndCert.passphrase}\u0000junk`;
  const withNulBuf = Buffer.from(withNul);
  const withNulAb = withNulBuf.buffer.slice(
    withNulBuf.byteOffset, withNulBuf.byteOffset + withNulBuf.byteLength);

  const passphrases = [
    withNul,
    withNulBuf,
    withNulAb,
    new DataView(withNulAb),
    new Uint8Array(withNulAb),
    // A trailing NUL is no more representable than an interior one.
    `${keyAndCert.passphrase}\u0000`,
    // And neither is a passphrase that is nothing but a NUL, which must not
    // be mistaken for the empty passphrase.
    '\u0000',
    Buffer.from([0]),
  ];

  for (const passphrase of passphrases) {
    assert.throws(
      () => crypto.parsePKCS12(bundle, { passphrase }),
      {
        code: 'ERR_INVALID_ARG_VALUE',
        message: /options\.passphrase.*must not contain null bytes/,
      });
  }

  // The prefix on its own still opens the bundle, which is precisely why the
  // truncation had to be rejected rather than tolerated.
  assert.strictEqual(
    crypto.parsePKCS12(bundle, { passphrase: keyAndCert.passphrase })
      .privateKey.type,
    'private');
}

{
  // Argument validation.
  assert.throws(() => crypto.parsePKCS12('a string'),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => crypto.parsePKCS12(Buffer.alloc(0), 'nope'),
                { code: 'ERR_INVALID_ARG_TYPE' });
}
