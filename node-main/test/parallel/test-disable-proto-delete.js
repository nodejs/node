// Flags: --disable-proto=delete

'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');
const { Worker, isMainThread } = require('worker_threads');

// eslint-disable-next-line no-proto
assert.strictEqual(Object.prototype.__proto__, undefined);
assert(!Object.hasOwn(Object.prototype, '__proto__'));

const ctx = vm.createContext();
const ctxGlobal = vm.runInContext('this', ctx);

// eslint-disable-next-line no-proto
assert.strictEqual(ctxGlobal.Object.prototype.__proto__, undefined);
assert(!Object.hasOwn(ctxGlobal.Object.prototype, '__proto__'));

if (isMainThread) {
  new Worker(__filename);
} else {
  process.exit();
}
