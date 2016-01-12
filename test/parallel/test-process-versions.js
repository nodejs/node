'use strict';
var common = require('../common');
var assert = require('assert');

var expected_keys = ['ares', 'http_parser', 'modules', 'node',
                     'uv', 'zlib'];
expected_keys.push(process.jsEngine);

if (common.hasCrypto) {
  expected_keys.push('openssl');
}

if (!common.isChakraEngine && typeof Intl !== 'undefined') {
  expected_keys.push('icu');
}

assert.deepEqual(Object.keys(process.versions).sort(), expected_keys.sort());
