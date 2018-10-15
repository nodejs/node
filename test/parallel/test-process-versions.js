'use strict';
const common = require('../common');
const assert = require('assert');

const expected_keys = ['ares', 'http_parser', 'modules', 'node',
                       'uv', 'v8', 'zlib', 'nghttp2', 'napi'];

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
assert(commonTemplate.test(process.versions.http_parser));
assert(commonTemplate.test(process.versions.node));
assert(commonTemplate.test(process.versions.uv));
assert(commonTemplate.test(process.versions.zlib));

assert(/^\d+\.\d+\.\d+(?:\.\d+)?-node\.\d+(?: \(candidate\))?$/
  .test(process.versions.v8));
assert(/^\d+$/.test(process.versions.modules));

if (common.hasCrypto) {
  // example: 1.1.0i
  assert(/^\d+\.\d+\.\d+[a-z]?$/.test(process.versions.openssl));
}

// example: 3
assert(/^\d+$/.test(process.versions.napi));
// example: 1.34.0
assert(/^\d+\.\d+\.\d+$/.test(process.versions.nghttp2));

if (common.hasIntl) {
  // example: 2018e
  assert(/^\d{4}[a-z]$/.test(process.versions.tz));
  // example: 33.1
  assert(/^\d+\.\d+$/.test(process.versions.cldr));
  // example: 62.1
  assert(/^\d+\.\d+$/.test(process.versions.icu));
  // example: 11.0
  assert(/^\d+\.\d+$/.test(process.versions.unicode));
}

for (let i = 0; i < expected_keys.length; i++) {
  const key = expected_keys[i];
  const descriptor = Object.getOwnPropertyDescriptor(process.versions, key);
  assert.strictEqual(descriptor.writable, false);
}
