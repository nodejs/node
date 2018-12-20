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
const util = require('util');
const buffer = require('buffer');

buffer.INSPECT_MAX_BYTES = 2;

let b = Buffer.allocUnsafe(4);
b.fill('1234');

let s = buffer.SlowBuffer(4);
s.fill('1234');

let expected = '<Buffer 31 32 ... 2 more bytes>';

assert.strictEqual(util.inspect(b), expected);
assert.strictEqual(util.inspect(s), expected);

b = Buffer.allocUnsafe(2);
b.fill('12');

s = buffer.SlowBuffer(2);
s.fill('12');

expected = '<Buffer 31 32>';

assert.strictEqual(util.inspect(b), expected);
assert.strictEqual(util.inspect(s), expected);

buffer.INSPECT_MAX_BYTES = Infinity;

assert.strictEqual(util.inspect(b), expected);
assert.strictEqual(util.inspect(s), expected);

b.inspect = undefined;
assert.strictEqual(util.inspect(b), '<Buffer 31 32, inspect: undefined>');
