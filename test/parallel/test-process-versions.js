'use strict';
const common = require('../common');
const assert = require('assert');

var expected_keys = ['ares', 'http_parser', 'modules', 'node',
                     'uv', 'v8', 'zlib'];

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
