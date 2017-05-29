'use strict';
const common = require('../common');
const assert = require('assert');

const expected_keys = ['ares', 'http_parser', 'modules', 'node',
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

assert(/^\d+\.\d+\.\d+(-.*)?$/.test(process.versions.ares));
assert(/^\d+\.\d+\.\d+(-.*)?$/.test(process.versions.http_parser));
assert(/^\d+\.\d+\.\d+(-.*)?$/.test(process.versions.node));
assert(/^\d+\.\d+\.\d+(-.*)?$/.test(process.versions.uv));
assert(/^\d+\.\d+\.\d+(-.*)?$/.test(process.versions.zlib));
assert(/^\d+\.\d+\.\d+(\.\d+)?( \(candidate\))?$/.test(process.versions.v8));
assert(/^\d+$/.test(process.versions.modules));
