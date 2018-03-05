'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');

const { ECDH, getCurves } = require('crypto');

// A valid private key for the secp256k1 curve.
const cafebabeKey = 'cafebabe'.repeat(8);
// Associated compressed and uncompressed public keys (points).
const cafebabePubPtComp =
    '03672a31bfc59d3f04548ec9b7daeeba2f61814e8ccc40448045007f5479f693a3';
const cafebabePubPtUnComp =
    '04672a31bfc59d3f04548ec9b7daeeba2f61814e8ccc40448045007f5479f693a3' +
    '2e02c7f93d13dc2732b760ca377a5897b9dd41a1c1b29dc0442fdce6d0a04d1d';

// Invalid test: key argument is undefined.
common.expectsError(
  () => ECDH.convertKey(),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
  });

// Invalid test: curve argument is undefined.
common.expectsError(
  () => ECDH.convertKey(cafebabePubPtComp),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
  });

// Invalid test: curve argument is invalid.
common.expectsError(
  () => ECDH.convertKey(cafebabePubPtComp, 'badcurve'),
  {
    type: TypeError,
    message: 'Invalid ECDH curve name'
  });

if (getCurves().includes('secp256k1')) {
  // Invalid test: format argument is undefined.
  common.expectsError(
    () => ECDH.convertKey(cafebabePubPtComp, 'secp256k1', 'hex', 'hex', 10),
    {
      code: 'ERR_CRYPTO_ECDH_INVALID_FORMAT',
      type: TypeError,
      message: 'Invalid ECDH format: 10'
    });

  // Point formats.
  let uncompressed = ECDH.convertKey(cafebabePubPtComp,
                                     'secp256k1',
                                     'hex',
                                     'buffer',
                                     'uncompressed');
  let compressed = ECDH.convertKey(cafebabePubPtComp,
                                   'secp256k1',
                                   'hex',
                                   'buffer',
                                   'compressed');
  let hybrid = ECDH.convertKey(cafebabePubPtComp,
                               'secp256k1',
                               'hex',
                               'buffer',
                               'hybrid');
  assert.strictEqual(uncompressed[0], 4);
  let firstByte = compressed[0];
  assert(firstByte === 2 || firstByte === 3);
  firstByte = hybrid[0];
  assert(firstByte === 6 || firstByte === 7);

  // Format conversion from hex to hex
  uncompressed = ECDH.convertKey(cafebabePubPtComp,
                                 'secp256k1',
                                 'hex',
                                 'hex',
                                 'uncompressed');
  compressed = ECDH.convertKey(cafebabePubPtComp,
                               'secp256k1',
                               'hex',
                               'hex',
                               'compressed');
  hybrid = ECDH.convertKey(cafebabePubPtComp,
                           'secp256k1',
                           'hex',
                           'hex',
                           'hybrid');
  assert.strictEqual(uncompressed, cafebabePubPtUnComp);
  assert.strictEqual(compressed, cafebabePubPtComp);

  // Compare to getPublicKey.
  const ecdh1 = ECDH('secp256k1');
  ecdh1.generateKeys();
  ecdh1.setPrivateKey(cafebabeKey, 'hex');
  assert.strictEqual(ecdh1.getPublicKey('hex', 'uncompressed'), uncompressed);
  assert.strictEqual(ecdh1.getPublicKey('hex', 'compressed'), compressed);
  assert.strictEqual(ecdh1.getPublicKey('hex', 'hybrid'), hybrid);
}
