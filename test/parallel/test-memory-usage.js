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

// Flags: --predictable-gc-schedule
'use strict';
const common = require('../common');
const assert = require('assert');

const r = process.memoryUsage();
// On IBMi, the rss memory always returns zero
if (!common.isIBMi)
  assert.ok(r.rss > 0);
assert.ok(r.heapTotal > 0);
assert.ok(r.heapUsed > 0);
assert.ok(r.external > 0);

assert.strictEqual(typeof r.arrayBuffers, 'number');
if (r.arrayBuffers > 0) {
  const size = 10 * 1024 * 1024;
  // eslint-disable-next-line no-unused-vars
  const ab = new ArrayBuffer(size);

  const after = process.memoryUsage();
  assert(after.external - r.external >= size,
         `${after.external} - ${r.external} >= ${size}`);
  assert.strictEqual(after.arrayBuffers - r.arrayBuffers, size,
                     `${after.arrayBuffers} - ${r.arrayBuffers} === ${size}`);
}
