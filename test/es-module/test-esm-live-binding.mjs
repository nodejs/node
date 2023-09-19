import '../common/index.mjs';
import assert from 'assert';
import { syncBuiltinESMExports } from 'module';

import fs, { readFile, readFileSync } from 'fs';
import events, { defaultMaxListeners } from 'events';
import util from 'util';

const readFileDescriptor = Reflect.getOwnPropertyDescriptor(fs, 'readFile');
const readFileSyncDescriptor =
  Reflect.getOwnPropertyDescriptor(fs, 'readFileSync');

const s = Symbol();
const fn = () => s;

Reflect.deleteProperty(fs, 'readFile');
syncBuiltinESMExports();

assert.deepStrictEqual([fs.readFile, readFile], [undefined, undefined]);

fs.readFile = fn;
syncBuiltinESMExports();

assert.deepStrictEqual([fs.readFile(), readFile()], [s, s]);

Reflect.defineProperty(fs, 'readFile', {
  value: fn,
  configurable: true,
  writable: true,
});
syncBuiltinESMExports();

assert.deepStrictEqual([fs.readFile(), readFile()], [s, s]);

Reflect.deleteProperty(fs, 'readFile');
syncBuiltinESMExports();

assert.deepStrictEqual([fs.readFile, readFile], [undefined, undefined]);

let count = 0;

Reflect.defineProperty(fs, 'readFile', {
  get() { return count; },
  configurable: true,
});
syncBuiltinESMExports();

assert.deepStrictEqual([readFile, fs.readFile], [0, 0]);

count++;
syncBuiltinESMExports();

assert.deepStrictEqual([fs.readFile, readFile], [1, 1]);

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
syncBuiltinESMExports();

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
syncBuiltinESMExports();

assert.strictEqual(defaultMaxListeners, originDefaultMaxListeners);
assert.strictEqual(events.defaultMaxListeners, originDefaultMaxListeners);

events.defaultMaxListeners += 1;

assert.strictEqual(events.defaultMaxListeners,
                   originDefaultMaxListeners + 1);

syncBuiltinESMExports();

assert.strictEqual(defaultMaxListeners, originDefaultMaxListeners + 1);
assert.strictEqual(Function.prototype.defaultMaxListeners, 1);

Function.prototype.defaultMaxListeners = 'foo';
syncBuiltinESMExports();

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
syncBuiltinESMExports();

assert.strictEqual(util.foo, 1);

util.foo = 'bar';
syncBuiltinESMExports();

assert.strictEqual(count, 1);
assert.strictEqual(util.foo, 'bar');
assert.strictEqual(p.foo, 2);

p.foo = 'foo';
syncBuiltinESMExports();

assert.strictEqual(p.foo, 'foo');

events.defaultMaxListeners = originDefaultMaxListeners;
util.__proto__ = utilProto; // eslint-disable-line no-proto

Reflect.deleteProperty(util, 'foo');
Reflect.deleteProperty(Function.prototype, 'defaultMaxListeners');
syncBuiltinESMExports();

assert.throws(
  () => Object.defineProperty(events, 'defaultMaxListeners', { value: 3 }),
  /TypeError: Cannot redefine/
);
