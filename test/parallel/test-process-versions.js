'use strict';
const common = require('../common');
const assert = require('assert');
const util = require('util');

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
assert(/^\d+\.\d+\.\d+(\.\d+)?$/.test(process.versions.v8));
assert(/^\d+$/.test(process.versions.modules));

const testInspectCustom = () => {
  const all = process.versions[util.inspect.custom]();
  assert(all.startsWith('{ '));
  assert(all.endsWith(' }'));
  expected_keys.every((key) => {
    const value = process.versions[key];
    assert(all.includes(` ${key}: '${value}'`),
           `Cannot find expected value ${value} for ${key} in ${all}`);
  });
};

testInspectCustom();

// run tests again with monkey-patched util.format()
{
  const saveFormat = util.format;
  util.format = () => {
    assert.fail('monkey-patched util.format() should not be invoked');
  };

  testInspectCustom();

  // restore util.format to avoid side effects
  util.format = saveFormat;
}
