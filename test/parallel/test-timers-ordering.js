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

// Flags: --expose-internals

'use strict';
require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const binding = internalBinding('timers');

const N = 30;

let last_i = 0;
let last_ts = 0;

function f(i) {
  if (i <= N) {
    // check order
    assert.strictEqual(i, last_i + 1, `order is broken: ${i} != ${last_i} + 1`);
    last_i = i;

    // Check that this iteration is fired at least 1ms later than the previous
    // We need to use the binding as the receiver for fast API calls.
    const now = binding.getLibuvNow();
    assert(now >= last_ts + 1,
           `current ts ${now} < prev ts ${last_ts} + 1`);
    last_ts = now;

    // Schedule next iteration
    setTimeout(f, 1, i + 1);
  }
}
setTimeout(f, 1, 1);
