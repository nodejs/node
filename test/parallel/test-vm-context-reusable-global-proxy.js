'use strict';

require('../common');

const assert = require('assert');
const vm = require('vm');

const globalProxy = vm.createContext(vm.constants.DONT_CONTEXTIFY, { name: 'realm 1' });

assert.strictEqual(vm.runInContext('globalThis', globalProxy), globalProxy);
assert.strictEqual(vm.isContext(globalProxy), true);

vm.runInContext('globalThis.marker = "realm 1"', globalProxy);
const realm1Array = vm.runInContext('Array', globalProxy);
const realm1Function = vm.runInContext('() => marker', globalProxy);
const realm1GlobalThis = vm.runInContext('() => globalThis', globalProxy);

const independentGlobalProxy = vm.createContext(vm.constants.DONT_CONTEXTIFY, { name: 'realm 3' });
vm.runInContext('globalThis.marker = "realm 3"', independentGlobalProxy);

const recreated = vm.createContext(globalProxy, { name: 'realm 2', reuseGlobalProxy: true });

assert.strictEqual(recreated, globalProxy);
assert.strictEqual(vm.runInContext('globalThis', globalProxy), globalProxy);
assert.strictEqual(vm.isContext(globalProxy), true);
assert.strictEqual(vm.runInContext('typeof marker', globalProxy), 'undefined');
assert.notStrictEqual(vm.runInContext('Array', globalProxy), realm1Array);
assert.strictEqual(realm1Function(), 'realm 1');
assert.strictEqual(realm1GlobalThis(), globalProxy);
assert.strictEqual(vm.runInContext('marker', independentGlobalProxy), 'realm 3');

const realm2Array = vm.runInContext('Array', globalProxy);
assert.strictEqual(vm.createContext(globalProxy, { name: 'realm 4', reuseGlobalProxy: true }), globalProxy);
assert.notStrictEqual(vm.runInContext('Array', globalProxy), realm2Array);
assert.strictEqual(realm1Function(), 'realm 1');

const ordinaryContext = vm.createContext(vm.constants.DONT_CONTEXTIFY);
const ordinaryArray = vm.runInContext('Array', ordinaryContext);

assert.strictEqual(vm.createContext(ordinaryContext), ordinaryContext);
assert.strictEqual(vm.runInContext('Array', ordinaryContext), ordinaryArray);

assert.throws(
  () => vm.createContext({}, { reuseGlobalProxy: true }), { code: 'ERR_INVALID_ARG_VALUE' });

const contextifiedSandbox = vm.createContext({ marker: 'contextified' });
assert.throws(
  () => vm.createContext(contextifiedSandbox, { reuseGlobalProxy: true }), { code: 'ERR_INVALID_ARG_VALUE' });
assert.strictEqual(vm.runInContext('marker', contextifiedSandbox), 'contextified');

const microtaskGlobalProxy = vm.createContext(vm.constants.DONT_CONTEXTIFY, { microtaskMode: 'afterEvaluate' });
vm.runInContext('Promise.resolve().then(() => globalThis.marker = "realm 1")', microtaskGlobalProxy);
assert.strictEqual(vm.runInContext('marker', microtaskGlobalProxy), 'realm 1');

vm.createContext(microtaskGlobalProxy, { reuseGlobalProxy: true, microtaskMode: 'afterEvaluate' });
assert.strictEqual(vm.runInContext('typeof marker', microtaskGlobalProxy), 'undefined');
vm.runInContext('Promise.resolve().then(() => globalThis.marker = "realm 2")', microtaskGlobalProxy);
assert.strictEqual(vm.runInContext('marker', microtaskGlobalProxy), 'realm 2');
