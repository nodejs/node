// Flags: --experimental-modules

import '../common';
import assert from 'assert';

import fs, { readFile, readFileSync } from 'fs';
import events, { defaultMaxListeners } from 'events';
import util from 'util';

const readFileDescriptor = Reflect.getOwnPropertyDescriptor(fs, 'readFile');
const readFileSyncDescriptor =
  Reflect.getOwnPropertyDescriptor(fs, 'readFileSync');

const s = Symbol();
const fn = () => s;

Reflect.deleteProperty(fs, 'readFile');

assert.deepStrictEqual([fs.readFile, readFile], [undefined, undefined]);

fs.readFile = fn;

assert.deepStrictEqual([fs.readFile(), readFile()], [s, s]);

Reflect.defineProperty(fs, 'readFile', {
  value: fn,
  configurable: true,
  writable: true,
});

assert.deepStrictEqual([fs.readFile(), readFile()], [s, s]);

Reflect.deleteProperty(fs, 'readFile');

assert.deepStrictEqual([fs.readFile, readFile], [undefined, undefined]);

let count = 0;

Reflect.defineProperty(fs, 'readFile', {
  get() { return count; },
  configurable: true,
});

count++;

assert.deepStrictEqual([readFile, fs.readFile, readFile], [0, 1, 1]);

let otherValue;

Reflect.defineProperty(fs, 'readFile', { // eslint-disable-line accessor-pairs
  set(value) {
    Reflect.deleteProperty(fs, 'readFile');
    otherValue = value;
  },
  configurable: true,
});

Reflect.defineProperty(fs, 'readFileSync', {
  get() {
    return otherValue;
  },
  configurable: true,
});

fs.readFile = 2;

assert.deepStrictEqual([readFile, readFileSync], [undefined, 2]);

Reflect.defineProperty(fs, 'readFile', readFileDescriptor);
Reflect.defineProperty(fs, 'readFileSync', readFileSyncDescriptor);

const originDefaultMaxListeners = events.defaultMaxListeners;
const utilProto = util.__proto__; // eslint-disable-line no-proto

count = 0;

Reflect.defineProperty(Function.prototype, 'defaultMaxListeners', {
  configurable: true,
  enumerable: true,
  get: function() { return ++count; },
  set: function(v) {
    Reflect.defineProperty(this, 'defaultMaxListeners', {
      configurable: true,
      enumerable: true,
      writable: true,
      value: v,
    });
  },
});

assert.strictEqual(defaultMaxListeners, originDefaultMaxListeners);
assert.strictEqual(events.defaultMaxListeners, originDefaultMaxListeners);

assert.strictEqual(++events.defaultMaxListeners,
                   originDefaultMaxListeners + 1);

assert.strictEqual(defaultMaxListeners, originDefaultMaxListeners + 1);
assert.strictEqual(Function.prototype.defaultMaxListeners, 1);

Function.prototype.defaultMaxListeners = 'foo';

assert.strictEqual(Function.prototype.defaultMaxListeners, 'foo');
assert.strictEqual(events.defaultMaxListeners, originDefaultMaxListeners + 1);
assert.strictEqual(defaultMaxListeners, originDefaultMaxListeners + 1);

count = 0;

const p = {
  get foo() { return ++count; },
  set foo(v) {
    Reflect.defineProperty(this, 'foo', {
      configurable: true,
      enumerable: true,
      writable: true,
      value: v,
    });
  },
};

util.__proto__ = p; // eslint-disable-line no-proto

assert.strictEqual(util.foo, 1);

util.foo = 'bar';

assert.strictEqual(count, 1);
assert.strictEqual(util.foo, 'bar');
assert.strictEqual(p.foo, 2);

p.foo = 'foo';

assert.strictEqual(p.foo, 'foo');

events.defaultMaxListeners = originDefaultMaxListeners;
util.__proto__ = utilProto; // eslint-disable-line no-proto

Reflect.deleteProperty(util, 'foo');
Reflect.deleteProperty(Function.prototype, 'defaultMaxListeners');

assert.throws(
  () => Object.defineProperty(events, 'defaultMaxListeners', { value: 3 }),
  /TypeError: Cannot redefine/
);
