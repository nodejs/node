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
const tmpdir = require('../common/tmpdir');

// This test ensures that `fs.truncate` opens the file with `r+` and not `w`,
// which had earlier resulted in the target file's content getting zeroed out.
// https://github.com/nodejs/node-v0.x-archive/issues/6233

const assert = require('assert');
const fs = require('fs');

const filename = `${tmpdir.path}/truncate-file.txt`;

tmpdir.refresh();

// Synchronous test.
{
  fs.writeFileSync(filename, '0123456789');
  assert.strictEqual(fs.readFileSync(filename).toString(), '0123456789');
  fs.truncateSync(filename, 5);
  assert.strictEqual(fs.readFileSync(filename).toString(), '01234');
}

// Asynchronous test.
{
  fs.writeFileSync(filename, '0123456789');
  assert.strictEqual(fs.readFileSync(filename).toString(), '0123456789');
  fs.truncate(
    filename,
    5,
    common.mustSucceed(() => {
      assert.strictEqual(fs.readFileSync(filename).toString(), '01234');
    })
  );
}
