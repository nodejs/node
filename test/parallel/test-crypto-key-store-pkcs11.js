'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const { hasOpenSSL } = require('../common/crypto');
if (!hasOpenSSL(3, 0))
  common.skip('requires OpenSSL 3.x');

// The PKCS#11 token, the OpenSSL configuration that activates a provider for
// it, and the PIN that unlocks it are all provided by the environment. See
// tools/nix/pkcs11.nix for the fixture this repository ships, which `shell.nix`
// exports when instantiated with `--arg pkcs11 true`.
const kOpenSSLConfig = process.env.NODE_TEST_PKCS11_OPENSSL_CONF;
const kPin = process.env.NODE_TEST_PKCS11_PIN;
if (!kOpenSSLConfig || !kPin)
  common.skip('missing a PKCS#11 provider test fixture');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const {
  constants: {
    RSA_PKCS1_PSS_PADDING,
  },
  createPublicKey,
  createPrivateKey,
  createSign,
  createVerify,
  diffieHellman,
  generateKeyPairSync,
  sign,
  verify,
} = require('crypto');
const tmpdir = require('../common/tmpdir');

const { subtle } = globalThis.crypto;
const kData = Buffer.from(
  Array.from({ length: 256 }, (_, i) => (i * 17 + 43) & 0xff));
const kProperties = 'provider=pkcs11';
const kExpectedPrivateExportFailure =
  /Failed to encode private key|Failed to export JWK|Failed to export RSA private key|Failed to export EC .* key|Failed to get raw .* key|keymgmt export failure|not exportable|operation not supported|not supported|incompatible/i;

// tools/nix/pkcs11.nix ships a SoftHSM directory holding the token and the
// configuration naming it. SoftHSM opens its token read-write, which a Nix
// store path can never be, so run from a writable copy of that directory; the
// configuration names the token relative to the working directory. A fixture
// configured by hand, a real HSM for instance, sets no directory and is used
// as it stands.
function softhsmOptions() {
  const source = process.env.NODE_TEST_PKCS11_SOFTHSM_DIR;
  if (!source) return {};

  tmpdir.refresh();
  const cwd = tmpdir.resolve('softhsm');
  fs.cpSync(source, cwd, { recursive: true });
  fs.chmodSync(cwd, 0o700);
  for (const entry of fs.readdirSync(cwd, { recursive: true })) {
    fs.chmodSync(path.join(cwd, entry), 0o700);
  }

  return { cwd, env: { SOFTHSM2_CONF: path.join(cwd, 'softhsm2.conf') } };
}

function runInChild() {
  const { cwd, env } = softhsmOptions();
  const child = spawnSync(process.execPath, [
    `--openssl-config=${kOpenSSLConfig}`,
    __filename,
  ], {
    cwd,
    env: { ...process.env, ...env, NODE_TEST_PKCS11_CHILD: '1' },
    stdio: 'inherit',
  });
  assert.strictEqual(child.status, 0);
}

function privateKeyUrl(label) {
  return new URL(`pkcs11:object=${label};type=private`);
}

function loadPrivateKey(label) {
  return createPrivateKey({
    key: privateKeyUrl(label),
    passphrase: kPin,
    properties: kProperties,
  });
}

function assertKeyDetails(key, type, asymmetricKeyType) {
  assert.strictEqual(key.type, type);
  assert.strictEqual(key.asymmetricKeyType, asymmetricKeyType);

  switch (asymmetricKeyType) {
    case 'rsa':
      assert.strictEqual(key.asymmetricKeyDetails.modulusLength, 2048);
      assert.strictEqual(key.asymmetricKeyDetails.publicExponent, 65537n);
      break;
    case 'ec':
      assert.strictEqual(key.asymmetricKeyDetails.namedCurve, 'prime256v1');
      break;
    case 'ed25519':
    case 'ed448':
      assert.deepStrictEqual(key.asymmetricKeyDetails, {});
      break;
    default:
      assert.fail(`unexpected asymmetric key type ${asymmetricKeyType}`);
  }
}

function assertDerivedPublicKey(privateKey, asymmetricKeyType) {
  const publicKey = createPublicKey(privateKey);
  assertKeyDetails(publicKey, 'public', asymmetricKeyType);
  return publicKey;
}

function assertPublicExports(publicKey) {
  const spkiPem = publicKey.export({ format: 'pem', type: 'spki' });
  assert.strictEqual(
    spkiPem.split('\n')[0],
    '-----BEGIN PUBLIC KEY-----');

  const spkiDer = publicKey.export({ format: 'der', type: 'spki' });
  assert(Buffer.isBuffer(spkiDer));
  assert(spkiDer.byteLength > 0);

  // The PEM must carry the same SubjectPublicKeyInfo the DER export produces.
  // Encoding a provider-backed key through OpenSSL's PEM_write_bio_PUBKEY()
  // yields a PKCS#1 body for RSA, which the label alone does not catch.
  const pemBody = Buffer.from(
    spkiPem.split('\n').filter((line) => !line.startsWith('---')).join(''),
    'base64');
  assert.deepStrictEqual(pemBody, spkiDer);
}

function assertPrivateExportsRejected(privateKey, asymmetricKeyType) {
  const specs = [
    { format: 'pem', type: 'pkcs8' },
    { format: 'der', type: 'pkcs8' },
    { format: 'jwk' },
  ];

  switch (asymmetricKeyType) {
    case 'rsa':
      specs.push(
        { format: 'pem', type: 'pkcs1' },
        { format: 'der', type: 'pkcs1' });
      break;
    case 'ec':
      specs.push(
        { format: 'pem', type: 'sec1' },
        { format: 'der', type: 'sec1' },
        { format: 'raw-private' });
      break;
    default:
      specs.push({ format: 'raw-private' });
  }

  for (const options of specs) {
    assert.throws(() => {
      privateKey.export(options);
    }, {
      message: kExpectedPrivateExportFailure,
    });
  }
}

function assertOneShotSignVerify(digest, data, privateKey, options = {}) {
  const publicKey = createPublicKey(privateKey);
  const signKey = { key: privateKey, ...options };
  const verifyPublicKey = { key: publicKey, ...options };
  const verifyPrivateKey = { key: privateKey, ...options };

  const signature = sign(digest, data, signKey);
  assert(signature.byteLength > 0);
  assert.strictEqual(verify(digest, data, verifyPublicKey, signature), true);
  assert.strictEqual(verify(digest, data, verifyPrivateKey, signature), true);

  return signature;
}

function assertStreamingSignOneShotVerify(digest, data, privateKey) {
  const publicKey = createPublicKey(privateKey);
  const signature = createSign(digest).update(data).sign(privateKey);
  assert(signature.byteLength > 0);
  assert.strictEqual(verify(digest, data, publicKey, signature), true);
  assert.strictEqual(verify(digest, data, privateKey, signature), true);

  assert.strictEqual(
    createVerify(digest).update(data).verify(publicKey, signature),
    true);
  assert.strictEqual(
    createVerify(digest).update(data).verify(privateKey, signature),
    true);
}

// The one-shot sign and verify callbacks run the operation on the threadpool
// rather than on the main thread. PKCS#11 sessions are shared process-wide, so
// exercise that path explicitly instead of only the synchronous one.
async function assertAsyncSignVerify(digest, data, privateKey) {
  const publicKey = createPublicKey(privateKey);

  const signature = await new Promise((resolve, reject) => {
    sign(digest, data, privateKey, (err, sig) => {
      if (err) reject(err);
      else resolve(sig);
    });
  });
  assert(signature.byteLength > 0);

  for (const key of [publicKey, privateKey]) {
    assert.strictEqual(await new Promise((resolve, reject) => {
      verify(digest, data, key, signature, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    }), true);
  }
}

// A store-backed key and a second load of the same URI are the same key.
function assertKeyObjectEquality(privateKey, label) {
  assert.strictEqual(privateKey.equals(loadPrivateKey(label)), true);
  assert.strictEqual(privateKey.equals(loadPrivateKey('node-ec')),
                     label === 'node-ec');
}

function assertEcdh(privateKey) {
  const peer = generateKeyPairSync('ec', { namedCurve: 'prime256v1' });

  // The peer public key has to reach OpenSSL through the SPKI decoder, which
  // is what happens in practice because a peer key arrives over the wire. A
  // public KeyObject that the default provider produced directly, including
  // createPublicKey() of this very key, is rejected by the PKCS#11 provider
  // with CKR_ARGUMENTS_BAD even though it is byte-for-byte the same key.
  const importSpki = (key) => createPublicKey({
    key: key.export({ format: 'der', type: 'spki' }),
    format: 'der',
    type: 'spki',
  });

  const publicKey = createPublicKey(privateKey);
  const ours = diffieHellman({
    privateKey,
    publicKey: importSpki(peer.publicKey),
  });
  const theirs = diffieHellman({
    privateKey: peer.privateKey,
    publicKey: importSpki(publicKey),
  });

  assert(ours.byteLength > 0);
  assert.deepStrictEqual(ours, theirs);
}

async function assertWebCryptoSignVerify(
  privateKey,
  publicKey,
  algorithm,
  privateUsages,
  publicUsages,
  signAlgorithm = algorithm.name,
) {
  const privateCryptoKey = privateKey.toCryptoKey(
    algorithm,
    false,
    privateUsages);
  assert.strictEqual(privateCryptoKey.type, 'private');
  assert.strictEqual(privateCryptoKey.extractable, false);
  assert.deepStrictEqual(privateCryptoKey.usages, privateUsages);

  await assert.rejects(
    subtle.exportKey('pkcs8', privateCryptoKey),
    {
      name: 'InvalidAccessError',
      message: /not extractable/i,
    });

  const publicCryptoKey = publicKey.toCryptoKey(
    algorithm,
    true,
    publicUsages);
  assert.strictEqual(publicCryptoKey.type, 'public');
  assert.strictEqual(publicCryptoKey.extractable, true);
  assert.deepStrictEqual(publicCryptoKey.usages, publicUsages);

  const signature = await subtle.sign(
    signAlgorithm,
    privateCryptoKey,
    kData);
  assert(signature instanceof ArrayBuffer);
  assert(signature.byteLength > 0);
  assert.strictEqual(
    await subtle.verify(
      signAlgorithm,
      publicCryptoKey,
      signature,
      kData),
    true);

  try {
    const exportedPublicKey = await subtle.exportKey('spki', publicCryptoKey);
    assert(exportedPublicKey instanceof ArrayBuffer);
    assert(exportedPublicKey.byteLength > 0);
  } catch (err) {
    assert.strictEqual(err.name, 'OperationError');
    assert.match(err.message, /operation-specific reason|not supported/i);
  }
}

async function assertPrivateCryptoKeyExportsRejected(
  privateKey,
  algorithm,
  privateUsages,
) {
  const privateCryptoKey = privateKey.toCryptoKey(
    algorithm,
    true,
    privateUsages);
  assert.strictEqual(privateCryptoKey.type, 'private');
  assert.strictEqual(privateCryptoKey.extractable, true);
  assert.deepStrictEqual(privateCryptoKey.usages, privateUsages);

  for (const format of ['pkcs8', 'jwk']) {
    await assert.rejects(
      subtle.exportKey(format, privateCryptoKey),
      (err) => {
        assert(err.name === 'OperationError' ||
               err.code === 'ERR_CRYPTO_OPERATION_FAILED');
        assert.match(err.cause?.message ?? err.message,
                     kExpectedPrivateExportFailure);
        return true;
      });
  }
}

function assertStoreOptions() {
  assert.strictEqual(
    createPrivateKey({
      key: privateKeyUrl('node-rsa'),
      passphrase: kPin,
    }).asymmetricKeyType,
    'rsa');

  assert.strictEqual(
    createPrivateKey({
      key: privateKeyUrl('node-rsa'),
      passphrase: kPin,
      properties: kProperties,
    }).asymmetricKeyType,
    'rsa');
}

function assertChild(args, expectedStatus, stderrPattern, options = {}) {
  const child = spawnSync(process.execPath, args, {
    env: process.env,
    encoding: 'utf8',
    ...options,
  });
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.status, expectedStatus, child.stderr || child.stdout);
  if (stderrPattern) assert.match(child.stderr, stderrPattern);
}

function assertStoreLoadFailure(code, stderrPattern, options) {
  assertChild([
    `--openssl-config=${kOpenSSLConfig}`,
    '-e',
    code,
  ], 1, stderrPattern, options);
}

function assertPassphraseHandling() {
  // When no passphrase is supplied the provider falls back to prompting for a
  // PIN through OpenSSL's default UI, which opens the terminal directly
  // (/dev/tty, or "con" on Windows) rather than reading stdin. Detaching gives
  // the child no controlling terminal, so the prompt cannot block. The result
  // is the same either way, because Node has already recorded that no
  // passphrase was available.
  assertStoreLoadFailure(`
    require('crypto').createPrivateKey({
      key: new URL('pkcs11:object=node-rsa;type=private'),
      properties: ${JSON.stringify(kProperties)},
    });
  `, /ERR_MISSING_PASSPHRASE/, { detached: true });

  assertStoreLoadFailure(`
    require('crypto').createPrivateKey({
      key: new URL('pkcs11:object=node-rsa;type=private'),
      passphrase: 'bad',
      properties: ${JSON.stringify(kProperties)},
    });
  `, /Failed to load private key through an OpenSSL STORE loader/);
}

function assertBadProperties() {
  assertStoreLoadFailure(`
    require('crypto').createPrivateKey({
      key: new URL('pkcs11:object=node-rsa;type=private'),
      passphrase: ${JSON.stringify(kPin)},
      properties: 'provider=default',
    });
  `, /Failed to load private key through an OpenSSL STORE loader|No such file or directory|unsupported/i);
}

function assertPermissionModel() {
  const code = `
    require('crypto').createPrivateKey({
      key: new URL('pkcs11:object=node-rsa;type=private'),
      passphrase: ${JSON.stringify(kPin)},
      properties: ${JSON.stringify(kProperties)},
    });
  `;

  assertChild([
    `--openssl-config=${kOpenSSLConfig}`,
    '--permission',
    '--allow-fs-read=*',
    '-e',
    code,
  ], 1, /ERR_ACCESS_DENIED/);

  assertChild([
    `--openssl-config=${kOpenSSLConfig}`,
    '--permission',
    '--allow-openssl-store',
    '--allow-fs-read=*',
    '-e',
    code,
  ], 0);
}

function assertInlineSignWithStoreUrl(privateKey) {
  const publicKey = createPublicKey(privateKey);
  const signature = sign('sha256', kData, {
    key: privateKeyUrl('node-rsa'),
    passphrase: kPin,
    properties: kProperties,
  });
  assert(signature.byteLength > 0);
  assert.strictEqual(verify('sha256', kData, publicKey, signature), true);
}

async function testRsa() {
  const privateKey = loadPrivateKey('node-rsa');
  assertKeyDetails(privateKey, 'private', 'rsa');

  const publicKey = assertDerivedPublicKey(privateKey, 'rsa');
  assertOneShotSignVerify('sha256', kData, privateKey);
  assertOneShotSignVerify('sha256', kData, privateKey, {
    padding: RSA_PKCS1_PSS_PADDING,
    saltLength: 32,
  });
  assertStreamingSignOneShotVerify('sha256', kData, privateKey);
  await assertAsyncSignVerify('sha256', kData, privateKey);
  assertKeyObjectEquality(privateKey, 'node-rsa');
  assertInlineSignWithStoreUrl(privateKey);
  assertPublicExports(publicKey);
  assertPrivateExportsRejected(privateKey, 'rsa');
  await assertPrivateCryptoKeyExportsRejected(
    privateKey,
    { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
    ['sign']);

  await assertWebCryptoSignVerify(
    privateKey,
    publicKey,
    { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
    ['sign'],
    ['verify']);

  await assertWebCryptoSignVerify(
    privateKey,
    publicKey,
    { name: 'RSA-PSS', hash: 'SHA-256' },
    ['sign'],
    ['verify'],
    { name: 'RSA-PSS', saltLength: 32 });
}

async function testEc() {
  const privateKey = loadPrivateKey('node-ec');
  assertKeyDetails(privateKey, 'private', 'ec');

  const publicKey = assertDerivedPublicKey(privateKey, 'ec');
  assertOneShotSignVerify('sha256', kData, privateKey);
  assertOneShotSignVerify('sha256', kData, privateKey, {
    dsaEncoding: 'ieee-p1363',
  });
  assertStreamingSignOneShotVerify('sha256', kData, privateKey);
  await assertAsyncSignVerify('sha256', kData, privateKey);
  assertKeyObjectEquality(privateKey, 'node-ec');

  assertPublicExports(publicKey);
  assertPrivateExportsRejected(privateKey, 'ec');
  await assertPrivateCryptoKeyExportsRejected(
    privateKey,
    { name: 'ECDSA', namedCurve: 'P-256' },
    ['sign']);
  await assertWebCryptoSignVerify(
    privateKey,
    publicKey,
    { name: 'ECDSA', namedCurve: 'P-256' },
    ['sign'],
    ['verify'],
    { name: 'ECDSA', hash: 'SHA-256' });
}

async function testEd25519() {
  const privateKey = loadPrivateKey('node-ed25519');
  assertKeyDetails(privateKey, 'private', 'ed25519');

  const publicKey = assertDerivedPublicKey(privateKey, 'ed25519');
  assertOneShotSignVerify(null, kData, privateKey);
  await assertAsyncSignVerify(null, kData, privateKey);
  assertPublicExports(publicKey);
  assertPrivateExportsRejected(privateKey, 'ed25519');
  await assertPrivateCryptoKeyExportsRejected(
    privateKey,
    { name: 'Ed25519' },
    ['sign']);

  await assertWebCryptoSignVerify(
    privateKey,
    publicKey,
    { name: 'Ed25519' },
    ['sign'],
    ['verify']);
}

function testEcDiffieHellman() {
  const privateKey = loadPrivateKey('node-ecdh');
  assertKeyDetails(privateKey, 'private', 'ec');

  const publicKey = assertDerivedPublicKey(privateKey, 'ec');
  assertPublicExports(publicKey);
  assertPrivateExportsRejected(privateKey, 'ec');
  assertEcdh(privateKey);
}

async function testEd448() {
  const privateKey = loadPrivateKey('node-ed448');
  assertKeyDetails(privateKey, 'private', 'ed448');

  const publicKey = assertDerivedPublicKey(privateKey, 'ed448');
  assertOneShotSignVerify(null, kData, privateKey);
  await assertAsyncSignVerify(null, kData, privateKey);
  assertPublicExports(publicKey);
  assertPrivateExportsRejected(privateKey, 'ed448');
  await assertPrivateCryptoKeyExportsRejected(
    privateKey,
    { name: 'Ed448' },
    ['sign']);

  await assertWebCryptoSignVerify(
    privateKey,
    publicKey,
    { name: 'Ed448' },
    ['sign'],
    ['verify']);
}

async function runTest() {
  assertStoreOptions();
  assertPassphraseHandling();
  assertBadProperties();
  assertPermissionModel();

  await testRsa();
  await testEc();
  testEcDiffieHellman();
  await testEd25519();
  await testEd448();
}

if (process.env.NODE_TEST_PKCS11_CHILD === '1') {
  runTest().then(common.mustCall()).catch((err) => {
    process.nextTick(() => {
      throw err;
    });
  });
} else {
  runInChild();
}
