// Flags: --experimental-modules

import '../common';
import assert from 'assert';

import fs, { readFile } from 'fs';

const s = Symbol();
const fn = () => s;

delete fs.readFile;
assert.strictEqual(fs.readFile, undefined);
assert.strictEqual(readFile, undefined);

fs.readFile = fn;

assert.strictEqual(fs.readFile(), s);
assert.strictEqual(readFile(), s);

Reflect.deleteProperty(fs, 'readFile');

Reflect.defineProperty(fs, 'readFile', {
  value: fn,
  configurable: true,
  writable: true,
});

assert.strictEqual(fs.readFile(), s);
assert.strictEqual(readFile(), s);

Reflect.deleteProperty(fs, 'readFile');
assert.strictEqual(fs.readFile, undefined);
assert.strictEqual(readFile, undefined);

Reflect.defineProperty(fs, 'readFile', {
  get() { return fn; },
  set() {},
  configurable: true,
});

assert.strictEqual(fs.readFile(), s);
assert.strictEqual(readFile(), s);
