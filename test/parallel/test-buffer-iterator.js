// Copyright io.js contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');

var buffer = new Buffer([1, 2, 3, 4, 5]);
var arr;
var b;

// buffers should be iterable

arr = [];

for (b of buffer)
    arr.push(b);

assert.deepEqual(arr, [1, 2, 3, 4, 5]);


// buffer iterators should be iterable

arr = [];

for (b of buffer[Symbol.iterator]())
  arr.push(b);

assert.deepEqual(arr, [1, 2, 3, 4, 5]);


// buffer#values() should return iterator for values

arr = [];

for (b of buffer.values())
  arr.push(b);

assert.deepEqual(arr, [1, 2, 3, 4, 5]);


// buffer#keys() should return iterator for keys

arr = [];

for (b of buffer.keys())
  arr.push(b);

assert.deepEqual(arr, [0, 1, 2, 3, 4]);


// buffer#entries() should return iterator for entries

arr = [];

for (var b of buffer.entries())
  arr.push(b);

assert.deepEqual(arr, [
  [0, 1],
  [1, 2],
  [2, 3],
  [3, 4],
  [4, 5]
]);
