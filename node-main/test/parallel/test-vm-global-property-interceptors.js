'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

const dSymbol = Symbol('d');
const sandbox = {
  a: 'a',
  dSymbol
};

Object.defineProperties(sandbox, {
  b: {
    value: 'b'
  },
  c: {
    value: 'c',
    writable: true,
    enumerable: true
  },
  [dSymbol]: {
    value: 'd'
  },
  e: {
    value: 'e',
    configurable: true
  },
  f: {}
});

const ctx = vm.createContext(sandbox);

const result = vm.runInContext(`
const getDesc = (prop) => Object.getOwnPropertyDescriptor(this, prop);
const result = {
  a: getDesc('a'),
  b: getDesc('b'),
  c: getDesc('c'),
  d: getDesc(dSymbol),
  e: getDesc('e'),
  f: getDesc('f'),
  g: getDesc('g')
};
result;
`, ctx);

// eslint-disable-next-line no-restricted-properties
assert.deepEqual(result, {
  a: { value: 'a', writable: true, enumerable: true, configurable: true },
  b: { value: 'b', writable: false, enumerable: false, configurable: false },
  c: { value: 'c', writable: true, enumerable: true, configurable: false },
  d: { value: 'd', writable: false, enumerable: false, configurable: false },
  e: { value: 'e', writable: false, enumerable: false, configurable: true },
  f: {
    value: undefined,
    writable: false,
    enumerable: false,
    configurable: false
  },
  g: undefined
});

// Define new properties
vm.runInContext(`
Object.defineProperty(this, 'h', {value: 'h'});
Object.defineProperty(this, 'i', {});
Object.defineProperty(this, 'j', {
  get() { return 'j'; }
});
let kValue = 0;
Object.defineProperty(this, 'k', {
  get() { return kValue; },
  set(value) { kValue = value }
});
`, ctx);

assert.deepStrictEqual(Object.getOwnPropertyDescriptor(ctx, 'h'), {
  value: 'h',
  writable: false,
  enumerable: false,
  configurable: false
});

assert.deepStrictEqual(Object.getOwnPropertyDescriptor(ctx, 'i'), {
  value: undefined,
  writable: false,
  enumerable: false,
  configurable: false
});

const jDesc = Object.getOwnPropertyDescriptor(ctx, 'j');
assert.strictEqual(typeof jDesc.get, 'function');
assert.strictEqual(typeof jDesc.set, 'undefined');
assert.strictEqual(jDesc.enumerable, false);
assert.strictEqual(jDesc.configurable, false);

const kDesc = Object.getOwnPropertyDescriptor(ctx, 'k');
assert.strictEqual(typeof kDesc.get, 'function');
assert.strictEqual(typeof kDesc.set, 'function');
assert.strictEqual(kDesc.enumerable, false);
assert.strictEqual(kDesc.configurable, false);

assert.strictEqual(ctx.k, 0);
ctx.k = 1;
assert.strictEqual(ctx.k, 1);
assert.strictEqual(vm.runInContext('k;', ctx), 1);
vm.runInContext('k = 2;', ctx);
assert.strictEqual(ctx.k, 2);
assert.strictEqual(vm.runInContext('k;', ctx), 2);

// Redefine properties on the global object
assert.strictEqual(typeof vm.runInContext('encodeURI;', ctx), 'function');
assert.strictEqual(ctx.encodeURI, undefined);
vm.runInContext(`
Object.defineProperty(this, 'encodeURI', { value: 42 });
`, ctx);
assert.strictEqual(vm.runInContext('encodeURI;', ctx), 42);
assert.strictEqual(ctx.encodeURI, 42);

// Redefine properties on the sandbox
vm.runInContext(`
Object.defineProperty(this, 'e', { value: 'newE' });
`, ctx);
assert.strictEqual(ctx.e, 'newE');

assert.throws(() => vm.runInContext(`
'use strict';
Object.defineProperty(this, 'f', { value: 'newF' });
`, ctx), /TypeError: Cannot redefine property: f/);
