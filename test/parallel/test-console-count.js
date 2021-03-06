'use strict';

require('../common');
const assert = require('assert');

const stdoutWrite = process.stdout.write;

let buf = '';

process.stdout.write = (string) => buf = string;

console.count();
assert.strictEqual(buf, 'default: 1\n');

// 'default' and undefined are equivalent
console.count('default');
assert.strictEqual(buf, 'default: 2\n');

console.count('a');
assert.strictEqual(buf, 'a: 1\n');

console.count('b');
assert.strictEqual(buf, 'b: 1\n');

console.count('a');
assert.strictEqual(buf, 'a: 2\n');

console.count();
assert.strictEqual(buf, 'default: 3\n');

console.count({});
assert.strictEqual(buf, '[object Object]: 1\n');

console.count(1);
assert.strictEqual(buf, '1: 1\n');

console.count(null);
assert.strictEqual(buf, 'null: 1\n');

console.count('null');
assert.strictEqual(buf, 'null: 2\n');

console.countReset();
console.count();
assert.strictEqual(buf, 'default: 1\n');

console.countReset('a');
console.count('a');
assert.strictEqual(buf, 'a: 1\n');

// countReset('a') only reset the a counter
console.count();
assert.strictEqual(buf, 'default: 2\n');

process.stdout.write = stdoutWrite;

// Symbol labels do not work. Only check that the `Error` is a `TypeError`. Do
// not check the message because it is different depending on the JavaScript
// engine.
assert.throws(
  () => console.count(Symbol('test')),
  TypeError);
assert.throws(
  () => console.countReset(Symbol('test')),
  TypeError);
