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
  'tls'
]));

for (const key of keys) {
  assert.strictEqual(typeof process.features[key], 'boolean');
}
