'use strict';
var common = require('../common');
var assert = require('assert');

var expected_keys = ['ares', 'http_parser', 'modules', 'node',
                     'uv', 'v8', 'zlib'];

if (common.hasCrypto) {
  expected_keys.push('openssl');
}

if (typeof Intl !== 'undefined') {
  expected_keys.push('icu');
}

assert.deepEqual(Object.keys(process.versions).sort(), expected_keys.sort());
