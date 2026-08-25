'use strict';

require('../common');

const assert = require('assert');
const vm = require('vm');

const globalProxy = vm.createContext(vm.constants.DONT_CONTEXTIFY);
const oldObjectCtor = vm.runInContext('Object', globalProxy);
const reusedGlobalProxy = vm.createContext(globalProxy, {
  reuseGlobalProxy: true,
});
const newObjectCtor = vm.runInContext('Object', reusedGlobalProxy);

// The new context reuses the global proxy.
assert.strictEqual(reusedGlobalProxy, globalProxy);

// The reused proxy remains globalThis.
assert.strictEqual(
  vm.runInContext('globalThis', reusedGlobalProxy),
  globalProxy,
);

// The new context has its own realm.
assert.notStrictEqual(
  newObjectCtor,
  oldObjectCtor,
);

// Validate unsupported context objects.
assert.throws(
  () => vm.createContext({}, { reuseGlobalProxy: true }),
  { code: 'ERR_INVALID_ARG_VALUE' },
);

const contextifiedSandbox = vm.createContext({ marker: 'contextified' });
assert.throws(
  () => vm.createContext(contextifiedSandbox, { reuseGlobalProxy: true }),
  { code: 'ERR_INVALID_ARG_VALUE' },
);
