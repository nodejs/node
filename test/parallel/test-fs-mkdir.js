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
const fs = require('fs');

function unlink(pathname) {
  try {
    fs.rmdirSync(pathname);
  } catch (e) {
  }
}

common.refreshTmpDir();

{
  const pathname = common.tmpDir + '/test1';

  unlink(pathname);

  fs.mkdir(pathname, common.mustCall(function(err) {
    assert.strictEqual(err, null);
    assert.strictEqual(common.fileExists(pathname), true);
  }));

  process.on('exit', function() {
    unlink(pathname);
  });
}

{
  const pathname = common.tmpDir + '/test2';

  unlink(pathname);

  fs.mkdir(pathname, 0o777, common.mustCall(function(err) {
    assert.strictEqual(err, null);
    assert.strictEqual(common.fileExists(pathname), true);
  }));

  process.on('exit', function() {
    unlink(pathname);
  });
}

{
  const pathname = common.tmpDir + '/test3';

  unlink(pathname);
  fs.mkdirSync(pathname);

  const exists = common.fileExists(pathname);
  unlink(pathname);

  assert.strictEqual(exists, true);
}

// Keep the event loop alive so the async mkdir() requests
// have a chance to run (since they don't ref the event loop).
process.nextTick(function() {});
