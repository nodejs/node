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
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const filename = path.join(tmpdir.path, 'write.txt');

tmpdir.refresh();

{
  const parameters = [Buffer.from('bár'), 0, Buffer.byteLength('bár')];

  // The first time fs.writeSync is called with all parameters provided.
  // After that, each pop in the cycle removes the final parameter. So:
  // - The 2nd time fs.writeSync with a buffer, without the length parameter.
  // - The 3rd time fs.writeSync with a buffer, without the offset and length
  //   parameters.
  while (parameters.length > 0) {
    const fd = fs.openSync(filename, 'w');

    let written = fs.writeSync(fd, '');
    assert.strictEqual(written, 0);

    fs.writeSync(fd, 'foo');

    written = fs.writeSync(fd, ...parameters);
    assert.ok(written > 3);
    fs.closeSync(fd);

    assert.strictEqual(fs.readFileSync(filename, 'utf-8'), 'foobár');

    parameters.pop();
  }
}
