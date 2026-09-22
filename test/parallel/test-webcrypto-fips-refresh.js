// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { getFips, setFips } = require('crypto');
const { internalBinding } = require('internal/test/binding');
const { getOptionValue } = require('internal/options');
if (!internalBinding('crypto').testFipsCrypto())
  common.skip('requires an active FIPS provider');
if (getOptionValue('--force-fips'))
  common.skip('FIPS mode cannot be changed when forced');

const initial = getFips();
try {
  for (const fips of [false, true, false]) {
    setFips(fips);
    assert.strictEqual(SubtleCrypto.supports('digest', {
      name: 'TurboSHAKE128', outputLength: 128,
    }), !fips);
    assert.strictEqual(SubtleCrypto.supports('digest', {
      name: 'KT128', outputLength: 128,
    }), !fips);
    assert.strictEqual(SubtleCrypto.supports('digest', {
      name: 'cSHAKE128', outputLength: 128, customization: new Uint8Array(1),
    }), !fips);
    assert.strictEqual(SubtleCrypto.supports('generateKey', {
      name: 'RSA-PSS', hash: 'SHA-256', modulusLength: 1024,
      publicExponent: new Uint8Array([1, 0, 1]),
    }), !fips);
  }
} finally {
  setFips(initial);
}
