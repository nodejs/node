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
const common = require('../common');
const assert = require('assert');


// test for leaked global detection
global.gc = 42;  // Not a valid global unless --expose_gc is set.
assert.deepStrictEqual(common.leakedGlobals(), ['gc']);
delete global.gc;


// common.mustCall() tests
assert.throws(function() {
  common.mustCall(function() {}, 'foo');
}, /^TypeError: Invalid expected value: foo$/);

assert.throws(function() {
  common.mustCall(function() {}, /foo/);
}, /^TypeError: Invalid expected value: \/foo\/$/);


// common.fail() tests
assert.throws(
  () => { common.fail('fhqwhgads'); },
  /^AssertionError: fhqwhgads$/
);
