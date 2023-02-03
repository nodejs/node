'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

const window = createWindow();

const descriptor =
  Object.getOwnPropertyDescriptor(window.globalProxy, 'onhashchange');

assert.strictEqual(typeof descriptor.get, 'function');
assert.strictEqual(typeof descriptor.set, 'function');
assert.strictEqual(descriptor.configurable, true);

// Regression test for GH-42962. This assignment should not throw.
window.globalProxy.onhashchange = () => {};

assert.strictEqual(window.globalProxy.onhashchange, 42);

function createWindow() {
  const obj = {};
  vm.createContext(obj);
  Object.defineProperty(obj, 'onhashchange', {
    get: common.mustCall(() => 42),
    set: common.mustCall(),
    configurable: true
  });

  obj.globalProxy = vm.runInContext('this', obj);

  return obj;
}
