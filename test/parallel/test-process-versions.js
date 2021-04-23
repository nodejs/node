'use strict';
const common = require('../common');
const assert = require('assert');

const expected_keys = [
  'ares',
  'brotli',
  'modules',
  'node',
  'uv',
  'v8',
  'zlib',
  'nghttp2',
  'napi',
  'llhttp',
];

if (common.hasCrypto) {
  expected_keys.push('openssl');
}

if (common.hasQuic) {
  expected_keys.push('ngtcp2');
  expected_keys.push('nghttp3');
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
assert(commonTemplate.test(process.versions.node));
assert(commonTemplate.test(process.versions.uv));
assert(commonTemplate.test(process.versions.zlib));

assert(/^\d+\.\d+\.\d+(?:\.\d+)?-node\.\d+(?: \(candidate\))?$/
  .test(process.versions.v8));
assert(/^\d+$/.test(process.versions.modules));

if (common.hasCrypto) {
  const versionRegex = common.hasOpenSSL3 ?
    // The following also matches a development version of OpenSSL 3.x which
    // can be in the format '3.0.0-alpha4-dev'. This can be handy when building
    // and linking against the main development branch of OpenSSL.
    /^\d+\.\d+\.\d+(?:[-+][a-z0-9]+)*$/ :
    /^\d+\.\d+\.\d+[a-z]?(\+quic)?(-fips)?$/;
  assert(versionRegex.test(process.versions.openssl));
}

for (let i = 0; i < expected_keys.length; i++) {
  const key = expected_keys[i];
  const descriptor = Object.getOwnPropertyDescriptor(process.versions, key);
  assert.strictEqual(descriptor.writable, false);
}

assert.strictEqual(process.config.variables.napi_build_version,
                   process.versions.napi);
