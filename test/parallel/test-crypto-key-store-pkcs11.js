'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const { hasOpenSSL } = require('../common/crypto');
if (!hasOpenSSL(3, 0))
  common.skip('requires OpenSSL 3.x');

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
  sign,
  verify,
} = require('crypto');
const tmpdir = require('../common/tmpdir');

const { subtle } = globalThis.crypto;
const kData = Buffer.from(
  Array.from({ length: 256 }, (_, i) => (i * 17 + 43) & 0xff));
const kProperties = 'provider=pkcs11';
const kOpenSSLConfig = process.env.NODE_TEST_PKCS11_OPENSSL_CONF;
const kPin = process.env.NODE_TEST_PKCS11_PIN;
const kExpectedPrivateExportFailure =
  /Failed to encode private key|Failed to export JWK|Failed to export EC .* key|Failed to get raw .* key|keymgmt export failure|not exportable|operation not supported|not supported|incompatible/i;

function run(command, args, options = {}) {
  const result = spawnSync(command, args, {
    encoding: 'utf8',
    ...options,
  });

  if (result.status !== 0) {
    const output = [
      result.error?.message,
      result.stdout,
      result.stderr,
    ].filter(Boolean).join('\n');
    assert.fail(`${command} ${args.join(' ')} failed\n${output}`);
  }

  return result.stdout.trim();
}

function commandExists(command) {
  const result = spawnSync(command, ['--version'], { stdio: 'ignore' });
  return result.status === 0;
}

function findNixBuild() {
  return [
    'nix-build',
    '/nix/var/nix/profiles/default/bin/nix-build',
    '/run/current-system/sw/bin/nix-build',
  ].find(commandExists);
}

function lastStorePath(output) {
  const lines = output.split(/\r?\n/);
  for (let i = lines.length - 1; i >= 0; i--) {
    if (lines[i].startsWith('/nix/store/')) return lines[i];
  }
}

function nixBuild(command, repo, expression) {
  const output = run(command, [
    '-I',
    `nixpkgs=${path.join(repo, 'tools/nix/pkgs.nix')}`,
    '-I',
    `node=${repo}`,
    '--no-out-link',
    '-E',
    expression,
  ]);
  const storePath = lastStorePath(output);
  assert(storePath, `nix-build did not print a store path:\n${output}`);
  return storePath;
}

function findFile(root, suffixes) {
  const stack = [root];
  while (stack.length > 0) {
    const dir = stack.pop();
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      const file = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        stack.push(file);
      } else if (suffixes.some((suffix) => file.endsWith(suffix))) {
        return file;
      }
    }
  }
}

function shouldCreateNixFixture() {
  // Enabled explicitly by the shared OpenSSL CI matrix.
  return process.env.NODE_TEST_PKCS11_NIX === '1';
}

function createNixFixture() {
  if (!shouldCreateNixFixture()) {
    common.skip('requires a configured PKCS#11 provider test fixture');
  }
  const nixBuildCommand = findNixBuild();
  if (nixBuildCommand === undefined) {
    assert.fail('requires nix-build for the PKCS#11 provider test fixture');
  }

  const repo = process.env.TAR_DIR || path.resolve(__dirname, '..', '..');
  const opensslExpression = `
    let
      pkgs = import <nixpkgs> {};
      openssl = (import <node/tools/nix/sharedLibDeps.nix> {
        inherit pkgs;
      }).openssl;
    in
  `;
  const pkcs11ProviderPackage = nixBuild(nixBuildCommand, repo, `
    ${opensslExpression}
      (pkgs.pkcs11-provider.override { inherit openssl; })
        .overrideAttrs (_: { doCheck = false; })
  `);
  const softhsmPackage = nixBuild(nixBuildCommand, repo, `
    ${opensslExpression}
      (pkgs.softhsm.override { inherit openssl; })
        .overrideAttrs (_: { doCheck = false; })
  `);
  const openscPackage = nixBuild(
    nixBuildCommand,
    repo,
    'let pkgs = import <nixpkgs> {}; in pkgs.opensc');

  const pkcs11ProviderModule = findFile(pkcs11ProviderPackage, [
    '/lib/ossl-modules/pkcs11.so',
    '/lib/ossl-modules/pkcs11.dylib',
  ]);
  const softhsmModule = findFile(softhsmPackage, [
    '/lib/softhsm/libsofthsm2.so',
    '/lib/softhsm/libsofthsm2.dylib',
  ]);
  const softhsm2Util = path.join(softhsmPackage, 'bin/softhsm2-util');
  const pkcs11Tool = path.join(openscPackage, 'bin/pkcs11-tool');
  for (const file of [
    pkcs11ProviderModule,
    softhsmModule,
    softhsm2Util,
    pkcs11Tool,
  ]) {
    assert(file && fs.existsSync(file), `missing PKCS#11 fixture file: ${file}`);
  }

  tmpdir.refresh();
  const work = tmpdir.resolve('pkcs11-store');
  const tokens = path.join(work, 'tokens');
  fs.mkdirSync(tokens, { recursive: true });

  const pin = '1234';
  const softhsmConf = path.join(work, 'softhsm2.conf');
  const opensslConf = path.join(work, 'openssl-pkcs11.cnf');
  fs.writeFileSync(softhsmConf, `
directories.tokendir = ${tokens}
objectstore.backend = file
log.level = ERROR
slots.removable = false
`);
  fs.writeFileSync(opensslConf, `
nodejs_conf = nodejs_init

[nodejs_init]
providers = provider_sect

[provider_sect]
default = default_sect
pkcs11 = pkcs11_sect

[default_sect]
activate = 1

[pkcs11_sect]
module = ${pkcs11ProviderModule}
pkcs11-module-path = ${softhsmModule}
pkcs11-module-quirks = no-deinit
activate = 1
`);

  const env = { ...process.env, SOFTHSM2_CONF: softhsmConf };
  run(softhsm2Util, [
    '--init-token',
    '--free',
    '--label',
    'node-test',
    '--pin',
    pin,
    '--so-pin',
    pin,
  ], { env });

  // Keep this fixture limited to key types that SoftHSM and pkcs11-provider can
  // both generate and operate. Node's PQC APIs require OpenSSL >= 3.5, but that
  // does not imply ML-DSA or ML-KEM support in this PKCS#11 stack.
  for (const [keyType, id, label] of [
    ['RSA:2048', '01', 'node-rsa'],
    ['EC:prime256v1', '02', 'node-ec'],
    ['EC:ED25519', '03', 'node-ed25519'],
    ['EC:ED448', '04', 'node-ed448'],
  ]) {
    run(pkcs11Tool, [
      '--module',
      softhsmModule,
      '--login',
      '--pin',
      pin,
      '--keypairgen',
      '--key-type',
      keyType,
      '--id',
      id,
      '--label',
      label,
      '--usage-sign',
    ], { env });
  }

  run(pkcs11Tool, [
    '--module',
    softhsmModule,
    '--login',
    '--pin',
    pin,
    '--keypairgen',
    '--key-type',
    'EC:prime256v1',
    '--id',
    '05',
    '--label',
    'node-ecdh',
    '--usage-derive',
  ], { env });

  return { opensslConf, pin, softhsmConf };
}

function getFixture() {
  if (process.env.NODE_TEST_PKCS11_OPENSSL_CONF &&
      process.env.NODE_TEST_PKCS11_PIN) {
    return {
      opensslConf: process.env.NODE_TEST_PKCS11_OPENSSL_CONF,
      pin: process.env.NODE_TEST_PKCS11_PIN,
      softhsmConf: process.env.SOFTHSM2_CONF,
    };
  }

  return createNixFixture();
}

function runInChild() {
  const fixture = getFixture();
  const child = spawnSync(process.execPath, [
    `--openssl-config=${fixture.opensslConf}`,
    __filename,
  ], {
    env: {
      ...process.env,
      NODE_TEST_PKCS11_CHILD: '1',
      NODE_TEST_PKCS11_OPENSSL_CONF: fixture.opensslConf,
      NODE_TEST_PKCS11_PIN: fixture.pin,
      ...(fixture.softhsmConf && { SOFTHSM2_CONF: fixture.softhsmConf }),
    },
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

function assertChild(args, expectedStatus, stderrPattern) {
  const child = spawnSync(process.execPath, args, {
    env: process.env,
    encoding: 'utf8',
  });
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.status, expectedStatus, child.stderr || child.stdout);
  if (stderrPattern) assert.match(child.stderr, stderrPattern);
}

function assertStoreLoadFailure(code, stderrPattern) {
  assertChild([
    `--openssl-config=${kOpenSSLConfig}`,
    '-e',
    code,
  ], 1, stderrPattern);
}

function assertPassphraseHandling() {
  assertStoreLoadFailure(`
    require('crypto').createPrivateKey({
      key: new URL('pkcs11:object=node-rsa;type=private'),
      properties: ${JSON.stringify(kProperties)},
    });
  `, /ERR_MISSING_PASSPHRASE/);

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
    '--allow-crypto-store',
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

function testEcDiffieHellmanExports() {
  const privateKey = loadPrivateKey('node-ecdh');
  assertKeyDetails(privateKey, 'private', 'ec');

  const publicKey = assertDerivedPublicKey(privateKey, 'ec');
  assertPublicExports(publicKey);
  assertPrivateExportsRejected(privateKey, 'ec');
}

async function testEd448() {
  const privateKey = loadPrivateKey('node-ed448');
  assertKeyDetails(privateKey, 'private', 'ed448');

  const publicKey = assertDerivedPublicKey(privateKey, 'ed448');
  assertOneShotSignVerify(null, kData, privateKey);
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
  testEcDiffieHellmanExports();
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
