// Flags: --disable-proto=warn

'use strict';

const common = require('../common');
const assert = require('assert');
const vm = require('vm');
const { Worker, isMainThread } = require('worker_threads');

common.expectWarning(
  'DeprecationWarning',
  'Object.prototype.__proto__ is deprecated and will be removed in a future ' +
    'version of Node.js. Use Object.getPrototypeOf() and ' +
    'Object.setPrototypeOf() instead.'
);

// eslint-disable-next-line no-proto
const proto1 = ({}).__proto__;
assert.strictEqual(proto1, Object.prototype);

const obj = {};
const newProto = { a: 1 };
// eslint-disable-next-line no-proto
obj.__proto__ = newProto;
assert.strictEqual(Object.getPrototypeOf(obj), newProto);

const ctx = vm.createContext();
assert.strictEqual(
  vm.runInContext('({}).__proto__ === Object.prototype', ctx),
  true
);
assert.strictEqual(
  vm.runInContext(`
    const obj = {};
    const newProto = { a: 1 };
    obj.__proto__ = newProto;
    Object.getPrototypeOf(obj) === newProto;
  `, ctx),
  true
);

if (isMainThread) {
  new Worker(__filename);
} else {
  process.exit();
}
