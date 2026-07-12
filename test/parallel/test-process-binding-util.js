'use strict';
require('../common');
const assert = require('assert');
const util = require('util');

const utilBinding = process.binding('util');
assert.deepStrictEqual(
  Object.keys(utilBinding).sort(),
  [
    'isAnyArrayBuffer',
    'isArrayBuffer',
    'isArrayBufferView',
    'isAsyncFunction',
    'isDataView',
    'isDate',
    'isExternal',
    'isMap',
    'isMapIterator',
    'isNativeError',
    'isPromise',
    'isRegExp',
    'isSet',
    'isSetIterator',
    'isTypedArray',
    'isUint8Array',
  ]);

for (const k of Object.keys(utilBinding)) {
  assert.strictEqual(utilBinding[k], util.types[k]);
}
