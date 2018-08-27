'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

const sandbox = {};
const proxyHandlers = {};
const contextifiedProxy = vm.createContext(new Proxy(sandbox, proxyHandlers));

// One must get the globals and manually assign it to our own global object, to
// mitigate against https://github.com/nodejs/node/issues/17465.
const contextThis = vm.runInContext('this', contextifiedProxy);
for (const prop of Reflect.ownKeys(contextThis)) {
  const descriptor = Object.getOwnPropertyDescriptor(contextThis, prop);
  Object.defineProperty(sandbox, prop, descriptor);
}

// Finally, activate the proxy.
const numCalled = {};
for (const hook of Reflect.ownKeys(Reflect)) {
  numCalled[hook] = 0;
  proxyHandlers[hook] = (...args) => {
    numCalled[hook]++;
    return Reflect[hook](...args);
  };
}

{
  // Make sure the `in` operator only calls `getOwnPropertyDescriptor` and not
  // `get`.
  // Refs: https://github.com/nodejs/node/issues/17480
  assert.strictEqual(vm.runInContext('"a" in this', contextifiedProxy), false);
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
  vm.runInContext(
    'const { getOwnPropertyDescriptor } = Object;',
    contextifiedProxy);

  for (const p of Reflect.ownKeys(numCalled)) {
    numCalled[p] = 0;
  }

  assert.strictEqual(
    vm.runInContext('getOwnPropertyDescriptor(this, "a")', contextifiedProxy),
    undefined);
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
