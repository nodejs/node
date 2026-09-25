'use strict';

require('../common');

const { getRawAsset, getAsset, getAssetAsBlob } = require('node:sea');
const assert = require('node:assert');

const invalidKeyArgs = [1, 1n, Symbol(), false, null, undefined, {}, []];
const notInSeaError = {
  code: 'ERR_NOT_IN_SINGLE_EXECUTABLE_APPLICATION',
  name: 'Error',
};

// getRawAsset
for (const arg of invalidKeyArgs) {
  assert.throws(() => getRawAsset(arg), { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' });
}
assert.throws(() => getRawAsset('foo'), notInSeaError);

// getAsset
for (const arg of invalidKeyArgs) {
  assert.throws(() => getAsset(arg), { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' });
}
for (const arg of [1, 1n, Symbol(), false, null, {}, []]) {
  assert.throws(() => getAsset('foo', arg), { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' });
}
assert.throws(() => getAsset('foo'), notInSeaError);
assert.throws(() => getAsset('foo', 'utf8'), notInSeaError);
assert.throws(() => getAsset('foo', undefined), notInSeaError);

// getAssetAsBlob
for (const arg of invalidKeyArgs) {
  assert.throws(() => getAssetAsBlob(arg), { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' });
}
assert.throws(() => getAssetAsBlob('foo'), notInSeaError);
assert.throws(() => getAssetAsBlob('foo', {}), notInSeaError);
assert.throws(() => getAssetAsBlob('foo', { type: 'text/plain' }), notInSeaError);
