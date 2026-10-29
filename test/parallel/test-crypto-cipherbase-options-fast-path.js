'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  createCipheriv,
  createDecipheriv,
} = require('crypto');

const key = Buffer.alloc(16);
const gcmIv = Buffer.alloc(12);
const cbcIv = Buffer.alloc(16);

for (const create of [createCipheriv, createDecipheriv]) {
  // None of these supply either of the extended cipher options.
  create('aes-128-gcm', key, gcmIv);
  create('aes-128-gcm', key, gcmIv, null);
  create('aes-128-gcm', key, gcmIv, {});
  create('aes-128-gcm', key, gcmIv, undefined);
  create('aes-128-gcm', key, gcmIv, { authTagLength: 16 });

  for (const options of [
    { ctsMode: null },
    { ctsMode: undefined },
    { xtsStandard: null },
    { xtsStandard: undefined },
    { ctsMode: null, xtsStandard: undefined },
  ]) {
    create('aes-128-gcm', key, gcmIv, options);
  }

  const accesses = [];
  create('aes-128-gcm', key, gcmIv, {
    get authTagLength() {
      accesses.push('authTagLength');
      return 16;
    },
    get ctsMode() {
      accesses.push('ctsMode');
      return null;
    },
    get xtsStandard() {
      accesses.push('xtsStandard');
      return undefined;
    },
  });
  assert.deepStrictEqual(
    accesses,
    ['authTagLength', 'ctsMode', 'xtsStandard']);

  const extendedAccesses = [];
  assert.throws(
    () => create('aes-128-cbc', key, cbcIv, {
      get authTagLength() {
        extendedAccesses.push('authTagLength');
        return null;
      },
      get ctsMode() {
        extendedAccesses.push('ctsMode');
        return 'CS1';
      },
      get xtsStandard() {
        extendedAccesses.push('xtsStandard');
        return null;
      },
    }),
    { code: 'ERR_CRYPTO_UNSUPPORTED_OPERATION' });
  assert.deepStrictEqual(
    extendedAccesses,
    ['authTagLength', 'ctsMode', 'xtsStandard']);

  const invalidAuthTagAccesses = [];
  assert.throws(() => create('aes-128-cbc', key, cbcIv, {
    get authTagLength() {
      invalidAuthTagAccesses.push('authTagLength');
      return -2;
    },
    get ctsMode() {
      invalidAuthTagAccesses.push('ctsMode');
      return undefined;
    },
  }), { code: 'ERR_INVALID_ARG_VALUE' });
  assert.deepStrictEqual(invalidAuthTagAccesses, ['authTagLength']);

  const invalidCtsTypeAccesses = [];
  assert.throws(() => create('aes-128-cbc', key, cbcIv, {
    get ctsMode() {
      invalidCtsTypeAccesses.push('ctsMode');
      return 1;
    },
    get xtsStandard() {
      invalidCtsTypeAccesses.push('xtsStandard');
      return undefined;
    },
  }), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.deepStrictEqual(invalidCtsTypeAccesses, ['ctsMode']);

  const invalidCtsValueAccesses = [];
  assert.throws(() => create('aes-128-cbc', key, cbcIv, {
    get ctsMode() {
      invalidCtsValueAccesses.push('ctsMode');
      return 'CS4';
    },
    get xtsStandard() {
      invalidCtsValueAccesses.push('xtsStandard');
      return undefined;
    },
  }), { code: 'ERR_INVALID_ARG_VALUE' });
  assert.deepStrictEqual(
    invalidCtsValueAccesses,
    ['ctsMode', 'xtsStandard']);

  assert.throws(
    () => create('aes-128-cbc', key, cbcIv, { xtsStandard: 'GB' }),
    { code: 'ERR_CRYPTO_UNSUPPORTED_OPERATION' });

  for (const ctsMode of ['', 'CS4']) {
    assert.throws(
      () => create('aes-128-cbc', key, cbcIv, { ctsMode }),
      { code: 'ERR_INVALID_ARG_VALUE' });
  }
  for (const xtsStandard of ['', 'IEEE-1619']) {
    assert.throws(
      () => create('aes-128-cbc', key, cbcIv, { xtsStandard }),
      { code: 'ERR_INVALID_ARG_VALUE' });
  }
}
