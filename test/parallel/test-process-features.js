'use strict';

require('../common');
const assert = require('assert');

const keys = new Set(Object.keys(process.features));

assert.deepStrictEqual(keys, new Set([
  'inspector',
  'debug',
  'uv',
  'ipv6',
  'tls_alpn',
  'tls_sni',
  'tls_ocsp',
  'tls',
  'cached_builtins',
]));

for (const key of keys) {
  const original = process.features[key];
  assert.strictEqual(typeof original, 'boolean');
  // Check that they are not mutable.
  assert.throws(() => {
    process.features[key] = !original;
  }, TypeError);
}
