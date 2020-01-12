// Copyright Joyent, Inc. and other Node contributors.
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

'use strict';
require('../common');
const assert = require('assert');

// The default behavior, return an Array "tuple" of numbers
const tuple = process.hrtime();

// Validate the default behavior
validateTuple(tuple);

// Validate that passing an existing tuple returns another valid tuple
validateTuple(process.hrtime(tuple));

// Test that only an Array may be passed to process.hrtime()
assert.throws(() => {
  process.hrtime(1);
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
  message: 'The "time" argument must be an instance of Array. Received type ' +
           'number (1)'
});
assert.throws(() => {
  process.hrtime([]);
}, {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
  message: 'The value of "time" is out of range. It must be 2. Received 0'
});
assert.throws(() => {
  process.hrtime([1]);
}, {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
  message: 'The value of "time" is out of range. It must be 2. Received 1'
});
assert.throws(() => {
  process.hrtime([1, 2, 3]);
}, {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
  message: 'The value of "time" is out of range. It must be 2. Received 3'
});

function validateTuple(tuple) {
  assert(Array.isArray(tuple));
  assert.strictEqual(tuple.length, 2);
  assert(Number.isInteger(tuple[0]));
  assert(Number.isInteger(tuple[1]));
}

const diff = process.hrtime([0, 1e9 - 1]);
assert(diff[1] >= 0); // https://github.com/nodejs/node/issues/4751
