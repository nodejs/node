'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

const globals = {};
const handlers = {};
const proxy = vm.createContext(new Proxy(globals, handlers));

// One must get the globals and manually assign it to our own global object, to
// mitigate against https://github.com/nodejs/node/issues/17465.
const globalProxy = vm.runInContext('this', proxy);
for (const k of Reflect.ownKeys(globalProxy)) {
  Object.defineProperty(globals, k, Object.getOwnPropertyDescriptor(globalProxy, k));
}
Reflect.setPrototypeOf(globals, Reflect.getPrototypeOf(globalProxy));

// Finally, activate the proxy.
const numCalled = {};
for (const p of Reflect.ownKeys(Reflect)) {
  numCalled[p] = 0;
  handlers[p] = (...args) => {
    numCalled[p]++;
    return Reflect[p](...args);
  };
}

{
  // Make sure the `in` operator only calls `getOwnPropertyDescriptor` and not
  // `get`.
  // Refs: https://github.com/nodejs/node/issues/17480
  assert.strictEqual(vm.runInContext('"a" in this', proxy), false);
  assert.deepStrictEqual(numCalled, {
    defineProperty: 0,
    deleteProperty: 0,
    apply: 0,
    construct: 0,
    get: 0,
    getOwnPropertyDescriptor: 1,
    getPrototypeOf: 0,
    has: 0,
    isExtensible: 0,
    ownKeys: 0,
    preventExtensions: 0,
    set: 0,
    setPrototypeOf: 0
  });
}

{
  // Make sure `Object.getOwnPropertyDescriptor` only calls
  // `getOwnPropertyDescriptor` and not `get`.
  // Refs: https://github.com/nodejs/node/issues/17481

  // Get and store the function in a lexically scoped variable to avoid
  // interfering with the actual test.
  vm.runInContext('const { getOwnPropertyDescriptor } = Object;', proxy);

  for (const p of Reflect.ownKeys(numCalled)) {
    numCalled[p] = 0;
  }

  assert.strictEqual(
    vm.runInContext('getOwnPropertyDescriptor(this, "a")', proxy), undefined);
  assert.deepStrictEqual(numCalled, {
    defineProperty: 0,
    deleteProperty: 0,
    apply: 0,
    construct: 0,
    get: 0,
    getOwnPropertyDescriptor: 1,
    getPrototypeOf: 0,
    has: 0,
    isExtensible: 0,
    ownKeys: 0,
    preventExtensions: 0,
    set: 0,
    setPrototypeOf: 0
  });
}
