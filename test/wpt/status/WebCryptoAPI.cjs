'use strict';

const os = require('node:os');

const s390x = os.arch() === 's390x';

module.exports = {
  'algorithm-discards-context.https.window.js': {
    'skip': 'Not relevant in Node.js context',
  },
  'historical.any.js': {
    'skip': 'Not relevant in Node.js context',
  },
  'sign_verify/eddsa_small_order_points.https.any.js': {
    'fail': {
      'note': 'see https://github.com/nodejs/node/issues/54572',
      'expected': [
        'Ed25519 Verification checks with small-order key of order - Test 1',
        'Ed25519 Verification checks with small-order key of order - Test 2',
        'Ed25519 Verification checks with small-order key of order - Test 12',
        'Ed25519 Verification checks with small-order key of order - Test 13',
        ...(s390x ? [] : [
          'Ed25519 Verification checks with small-order key of order - Test 0',
          'Ed25519 Verification checks with small-order key of order - Test 11',
        ]),
      ],
    },
  },
  'getRandomValues.any.js': {
    'fail': {
      'note': 'https://github.com/nodejs/node/issues/58987',
      'expected': [
        'Large length: Int8Array',
        'Large length: Int16Array',
        'Large length: Int32Array',
        'Large length: BigInt64Array',
        'Large length: Uint8Array',
        'Large length: Uint8ClampedArray',
        'Large length: Uint16Array',
        'Large length: Uint32Array',
        'Large length: BigUint64Array',
      ],
    },
  },
};
