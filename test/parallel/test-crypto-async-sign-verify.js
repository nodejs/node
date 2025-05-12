'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL3 } = require('../common/crypto');
const assert = require('assert');
const util = require('util');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');

function test(
  publicFixture,
  privateFixture,
  algorithm,
  deterministic,
  options
) {
  let publicPem = fixtures.readKey(publicFixture);
  let privatePem = fixtures.readKey(privateFixture);
  let privateKey = crypto.createPrivateKey(privatePem);
  let publicKey = crypto.createPublicKey(publicPem);
  const privateDer = {
    key: privateKey.export({ format: 'der', type: 'pkcs8' }),
    format: 'der',
    type: 'pkcs8',
    ...options
  };
  const publicDer = {
    key: publicKey.export({ format: 'der', type: 'spki' }),
    format: 'der',
    type: 'spki',
    ...options
  };

  if (options) {
    publicPem = { ...options, key: publicPem };
    privatePem = { ...options, key: privatePem };
    privateKey = { ...options, key: privateKey };
    publicKey = { ...options, key: publicKey };
  }

  const data = Buffer.from('Hello world');
  const expected = crypto.sign(algorithm, data, privateKey);

  for (const key of [privatePem, privateKey, privateDer]) {
    crypto.sign(algorithm, data, key, common.mustSucceed((actual) => {
      if (deterministic) {
        assert.deepStrictEqual(actual, expected);
      }

      assert.strictEqual(
        crypto.verify(algorithm, data, key, actual), true);
    }));
  }

  const verifyInputs = [
    publicPem, publicKey, publicDer, privatePem, privateKey, privateDer];
  for (const key of verifyInputs) {
    crypto.verify(algorithm, data, key, expected, common.mustSucceed(
      (verified) => assert.strictEqual(verified, true)));

    crypto.verify(algorithm, data, key, Buffer.from(''), common.mustSucceed(
      (verified) => assert.strictEqual(verified, false)));
  }
}

// RSA w/ default padding
test('rsa_public.pem', 'rsa_private.pem', 'sha256', true);
test('rsa_public.pem', 'rsa_private.pem', 'sha256', true,
     { padding: crypto.constants.RSA_PKCS1_PADDING });

// RSA w/ PSS_PADDING and default saltLength
test('rsa_public.pem', 'rsa_private.pem', 'sha256', false,
     { padding: crypto.constants.RSA_PKCS1_PSS_PADDING });
test('rsa_public.pem', 'rsa_private.pem', 'sha256', false,
     {
       padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
       saltLength: crypto.constants.RSA_PSS_SALTLEN_MAX_SIGN
     });

// RSA w/ PSS_PADDING and PSS_SALTLEN_DIGEST
test('rsa_public.pem', 'rsa_private.pem', 'sha256', false,
     {
       padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
       saltLength: crypto.constants.RSA_PSS_SALTLEN_DIGEST
     });

// ED25519
test('ed25519_public.pem', 'ed25519_private.pem', undefined, true);
// ED448
test('ed448_public.pem', 'ed448_private.pem', undefined, true);

// ECDSA w/ der signature encoding
test('ec_secp256k1_public.pem', 'ec_secp256k1_private.pem', 'sha384',
     false);
test('ec_secp256k1_public.pem', 'ec_secp256k1_private.pem', 'sha384',
     false, { dsaEncoding: 'der' });

// ECDSA w/ ieee-p1363 signature encoding
test('ec_secp256k1_public.pem', 'ec_secp256k1_private.pem', 'sha384', false,
     { dsaEncoding: 'ieee-p1363' });

// DSA w/ der signature encoding
test('dsa_public.pem', 'dsa_private.pem', 'sha256',
     false);
test('dsa_public.pem', 'dsa_private.pem', 'sha256',
     false, { dsaEncoding: 'der' });

// DSA w/ ieee-p1363 signature encoding
test('dsa_public.pem', 'dsa_private.pem', 'sha256', false,
     { dsaEncoding: 'ieee-p1363' });

// Test Parallel Execution w/ KeyObject is threadsafe in openssl3
{
  const publicKey = {
    key: crypto.createPublicKey(
      fixtures.readKey('ec_p256_public.pem')),
    dsaEncoding: 'ieee-p1363',
  };
  const privateKey = {
    key: crypto.createPrivateKey(
      fixtures.readKey('ec_p256_private.pem')),
    dsaEncoding: 'ieee-p1363',
  };

  const sign = util.promisify(crypto.sign);
  const verify = util.promisify(crypto.verify);

  const data = Buffer.from('hello world');

  Promise.all([
    sign('sha256', data, privateKey),
    sign('sha256', data, privateKey),
    sign('sha256', data, privateKey),
  ]).then(([signature]) => {
    return Promise.all([
      verify('sha256', data, publicKey, signature),
      verify('sha256', data, publicKey, signature),
      verify('sha256', data, publicKey, signature),
    ]).then(common.mustCall());
  })
  .catch(common.mustNotCall());
}

{
  const untrustedKey = `-----BEGIN PUBLIC KEY-----
MCowBQYDK2VuAyEA6pwGRbadNQAI/tYN8+/p/0/hbsdHfOEGr1ADiLVk/Gc=
-----END PUBLIC KEY-----`;
  const data = crypto.randomBytes(32);
  const signature = crypto.randomBytes(16);

  const expected = hasOpenSSL3 ?
    /operation not supported for this keytype/ : /no default digest/;

  crypto.verify(undefined, data, untrustedKey, signature, common.mustCall((err) => {
    assert.ok(err);
    assert.match(err.message, expected);
  }));
}

{
  const { privateKey } = crypto.generateKeyPairSync('rsa', {
    modulusLength: 512
  });
  crypto.sign('sha512', 'message', privateKey, common.mustCall((err) => {
    assert.ok(err);
    assert.match(err.message, /digest too big for rsa key/);
  }));
}
