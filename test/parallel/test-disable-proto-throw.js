// Flags: --disable-proto=throw

'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');
const { Worker, isMainThread } = require('worker_threads');

assert(Object.hasOwn(Object.prototype, '__proto__'));

assert.throws(() => {
  // eslint-disable-next-line no-proto,no-unused-expressions
  ({}).__proto__;
}, {
  code: 'ERR_PROTO_ACCESS'
});

assert.throws(() => {
  // eslint-disable-next-line no-proto
  ({}).__proto__ = {};
}, {
  code: 'ERR_PROTO_ACCESS',
});

const ctx = vm.createContext();

assert.throws(() => {
  vm.runInContext('({}).__proto__;', ctx);
}, {
  code: 'ERR_PROTO_ACCESS'
});

assert.throws(() => {
  vm.runInContext('({}).__proto__ = {};', ctx);
}, {
  code: 'ERR_PROTO_ACCESS',
});

if (isMainThread) {
  new Worker(__filename);
} else {
  process.exit();
}
