'use strict';
const common = require('../common');
const assert = require('assert');

const expected_keys = ['ares', 'brotli', 'modules', 'node',
                       'uv', 'v8', 'zlib', 'nghttp2', 'napi',
                       'http_parser', 'llhttp'];

if (common.hasCrypto) {
  expected_keys.push('openssl');
}

if (common.hasIntl) {
  expected_keys.push('icu');
  expected_keys.push('cldr');
  expected_keys.push('tz');
  expected_keys.push('unicode');
}

expected_keys.sort();
const actual_keys = Object.keys(process.versions).sort();

assert.deepStrictEqual(actual_keys, expected_keys);

const commonTemplate = /^\d+\.\d+\.\d+(?:-.*)?$/;

assert(commonTemplate.test(process.versions.ares));
assert(commonTemplate.test(process.versions.brotli));
assert(commonTemplate.test(process.versions.llhttp));
assert(commonTemplate.test(process.versions.http_parser));
assert(commonTemplate.test(process.versions.node));
assert(commonTemplate.test(process.versions.uv));
assert(commonTemplate.test(process.versions.zlib));

assert(/^\d+\.\d+\.\d+(?:\.\d+)?-node\.\d+(?: \(candidate\))?$/
  .test(process.versions.v8));
assert(/^\d+$/.test(process.versions.modules));

if (common.hasCrypto) {
  assert(/^\d+\.\d+\.\d+[a-z]?(-fips)?$/.test(process.versions.openssl));
}

for (let i = 0; i < expected_keys.length; i++) {
  const key = expected_keys[i];
  const descriptor = Object.getOwnPropertyDescriptor(process.versions, key);
  assert.strictEqual(descriptor.writable, false);
}
