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

// Test that unlink succeeds immediately after readFile completes.

const assert = require('assert');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

const fileName = tmpdir.resolve('test.bin');
const buf = Buffer.alloc(512 * 1024, 42);

tmpdir.refresh();

fs.writeFileSync(fileName, buf);

fs.readFile(fileName, common.mustSucceed((data) => {
  assert.strictEqual(data.length, buf.length);
  assert.strictEqual(buf[0], 42);

  // Unlink should not throw. This is part of the test. It used to throw on
  // Windows due to a bug.
  fs.unlinkSync(fileName);
}));
