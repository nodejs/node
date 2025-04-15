'use strict';

const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const { subtle } = require('crypto');
const { strictEqual } = require('assert');
const { inspect } = require('util');

const promise = subtle.importKey('raw', Buffer.from([1, 2, 3]), {
  name: 'HMAC', hash: 'SHA-256'
}, true, ['sign', 'verify']);

promise.then(common.mustCall((key) => {
  const inspected = inspect(Object.getPrototypeOf(key));
  strictEqual(
    inspected,
    'CryptoKey {\n' +
    '  type: undefined,\n' +
    '  extractable: undefined,\n' +
    '  algorithm: undefined,\n' +
    '  usages: undefined\n' +
    '}'
  );
}));
