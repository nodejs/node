'use strict';
const common = require('../common');
const assert = require('assert');

const expected_keys = ['ares', 'http_parser', 'modules', 'node',
                       'uv', 'v8', 'zlib', 'napi'];

if (common.hasCrypto) {
  expected_keys.push('openssl');
}

if (common.hasIntl) {
  expected_keys.push('icu');
}

expected_keys.sort();
const actual_keys = Object.keys(process.versions).sort();

assert.deepStrictEqual(actual_keys, expected_keys);
