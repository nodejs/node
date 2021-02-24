'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');

async function test(
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
  const signature = crypto.sign(algorithm, data, privateKey);

  for (const key of [privatePem, privateKey, privateDer]) {
    crypto.sign(algorithm, data, key, common.mustSucceed((actual) => {
      if (deterministic) {
        assert.deepStrictEqual(actual, signature);
      } else {
        assert.strictEqual(
          crypto.verify(algorithm, data, key, signature), true);
      }
    }));
  }

  for (const key of [publicPem, publicKey, publicDer]) {
    crypto.verify(algorithm, data, key, signature, common.mustSucceed(
      (actual) => assert.strictEqual(actual, true)));

    crypto.verify(algorithm, data, key, Buffer.from(''), common.mustSucceed(
      (actual) => assert.strictEqual(actual, false)));
  }
}

Promise.all([
  // RSA w/ default padding
  test('rsa_public.pem', 'rsa_private.pem', 'sha256', true),
  test('rsa_public.pem', 'rsa_private.pem', 'sha256', true,
       { padding: crypto.constants.RSA_PKCS1_PADDING }),

  // RSA w/ PSS_PADDING and default saltLength
  test('rsa_public.pem', 'rsa_private.pem', 'sha256', false,
       { padding: crypto.constants.RSA_PKCS1_PSS_PADDING }),
  test('rsa_public.pem', 'rsa_private.pem', 'sha256', false,
       {
         padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
         saltLength: crypto.constants.RSA_PSS_SALTLEN_MAX_SIGN
       }),

  // RSA w/ PSS_PADDING and PSS_SALTLEN_DIGEST
  test('rsa_public.pem', 'rsa_private.pem', 'sha256', false,
       {
         padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
         saltLength: crypto.constants.RSA_PSS_SALTLEN_DIGEST
       }),

  // ED25519
  test('ed25519_public.pem', 'ed25519_private.pem', undefined, true),
  // ED448
  test('ed448_public.pem', 'ed448_private.pem', undefined, true),

  // TODO: add dsaSigEnc to SignJob
  // ECDSA w/ der signature encoding
  // test('ec_secp256k1_public.pem', 'ec_secp256k1_private.pem', 'sha384',
  //      false),
  // test('ec_secp256k1_public.pem', 'ec_secp256k1_private.pem', 'sha384',
  //      false, { dsaEncoding: 'der' }),

  // ECDSA w/ ieee-p1363 signature encoding
  test('ec_secp256k1_public.pem', 'ec_secp256k1_private.pem', 'sha384', false,
       { dsaEncoding: 'ieee-p1363' }),
]).then(common.mustCall());
