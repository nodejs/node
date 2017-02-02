'use strict';
require('../common');
const assert = require('assert');
const util = require('util');

// Template tag function turning an error message into a RegExp
// for assert.throws()
function re(literals, ...values) {
  let result = literals[0];
  for (const [i, value] of values.entries()) {
    const str = util.inspect(value);
    // Need to escape special characters.
    result += str.replace(/[\\^$.*+?()[\]{}|=!<>:-]/g, '\\$&');
    result += literals[i + 1];
  }
  return new RegExp(`^AssertionError: ${result}$`);
}

// The following deepEqual tests might seem very weird.
// They just describe what it is now.
// That is why we discourage using deepEqual in our own tests.

// Turn off no-restricted-properties because we are testing deepEqual!
/* eslint-disable no-restricted-properties */

const arr = new Uint8Array([120, 121, 122, 10]);
const buf = Buffer.from(arr);
// They have different [[Prototype]]
assert.throws(() => assert.deepStrictEqual(arr, buf));
assert.doesNotThrow(() => assert.deepEqual(arr, buf));

const buf2 = Buffer.from(arr);
buf2.prop = 1;

assert.throws(() => assert.deepStrictEqual(buf2, buf));
assert.doesNotThrow(() => assert.deepEqual(buf2, buf));

const arr2 = new Uint8Array([120, 121, 122, 10]);
arr2.prop = 5;
assert.throws(() => assert.deepStrictEqual(arr, arr2));
assert.doesNotThrow(() => assert.deepEqual(arr, arr2));

const date = new Date('2016');

class MyDate extends Date {
  constructor(...args) {
    super(...args);
    this[0] = '1';
  }
}

const date2 = new MyDate('2016');

// deepEqual returns true as long as the time are the same,
// but deepStrictEqual checks own properties
assert.doesNotThrow(() => assert.deepEqual(date, date2));
assert.doesNotThrow(() => assert.deepEqual(date2, date));
assert.throws(() => assert.deepStrictEqual(date, date2),
              re`${date} deepStrictEqual ${date2}`);
assert.throws(() => assert.deepStrictEqual(date2, date),
              re`${date2} deepStrictEqual ${date}`);

class MyRegExp extends RegExp {
  constructor(...args) {
    super(...args);
    this[0] = '1';
  }
}

const re1 = new RegExp('test');
const re2 = new MyRegExp('test');

// deepEqual returns true as long as the regexp-specific properties
// are the same, but deepStrictEqual checks all properties
assert.doesNotThrow(() => assert.deepEqual(re1, re2));
assert.throws(() => assert.deepStrictEqual(re1, re2),
              re`${re1} deepStrictEqual ${re2}`);

// For these weird cases, deepEqual should pass (at least for now),
// but deepStrictEqual should throw.
const similar = new Set([
  {0: '1'},  // Object
  {0: 1},  // Object
  new String('1'),  // Object
  ['1'],  // Array
  [1],  // Array
  date2,  // Date with this[0] = '1'
  re2,  // RegExp with this[0] = '1'
  new Int8Array([1]), // Int8Array
  new Uint8Array([1]), // Uint8Array
  new Int16Array([1]), // Int16Array
  new Uint16Array([1]), // Uint16Array
  new Int32Array([1]), // Int32Array
  new Uint32Array([1]), // Uint32Array
  Buffer.from([1]),
  // Arguments {'0': '1'} is not here
  // See https://github.com/nodejs/node-v0.x-archive/pull/7178
]);

for (const a of similar) {
  for (const b of similar) {
    if (a !== b) {
      assert.doesNotThrow(() => assert.deepEqual(a, b));
      assert.throws(() => assert.deepStrictEqual(a, b),
                    re`${a} deepStrictEqual ${b}`);
    }
  }
}

/* eslint-enable */
