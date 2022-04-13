'use strict';
require('../common');
const assert = require('assert');

const buffer = Buffer.from([1, 2, 3, 4, 5]);
let arr;
let b;

// Buffers should be iterable

arr = [];

for (b of buffer)
  arr.push(b);

assert.deepStrictEqual(arr, [1, 2, 3, 4, 5]);


// Buffer iterators should be iterable

arr = [];

for (b of buffer[Symbol.iterator]())
  arr.push(b);

assert.deepStrictEqual(arr, [1, 2, 3, 4, 5]);


// buffer#values() should return iterator for values

arr = [];

for (b of buffer.values())
  arr.push(b);

assert.deepStrictEqual(arr, [1, 2, 3, 4, 5]);


// buffer#keys() should return iterator for keys

arr = [];

for (b of buffer.keys())
  arr.push(b);

assert.deepStrictEqual(arr, [0, 1, 2, 3, 4]);


// buffer#entries() should return iterator for entries

arr = [];

for (b of buffer.entries())
  arr.push(b);

assert.deepStrictEqual(arr, [
  [0, 1],
  [1, 2],
  [2, 3],
  [3, 4],
  [4, 5],
]);
