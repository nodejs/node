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
const fixtures = require('../common/fixtures');
if (!common.canCreateSymLink())
  common.skip('insufficient privileges');

const assert = require('assert');
const path = require('path');
const fs = require('fs');

let linkTime;
let fileTime;

common.refreshTmpDir();

// test creating and reading symbolic link
const linkData = fixtures.path('/cycles/root.js');
const linkPath = path.join(common.tmpDir, 'symlink1.js');

fs.symlink(linkData, linkPath, common.mustCall(function(err) {
  assert.ifError(err);

  fs.lstat(linkPath, common.mustCall(function(err, stats) {
    assert.ifError(err);
    linkTime = stats.mtime.getTime();
  }));

  fs.stat(linkPath, common.mustCall(function(err, stats) {
    assert.ifError(err);
    fileTime = stats.mtime.getTime();
  }));

  fs.readlink(linkPath, common.mustCall(function(err, destination) {
    assert.ifError(err);
    assert.strictEqual(destination, linkData);
  }));
}));

[false, 1, {}, [], null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.symlink(i, '', common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message:
        'The "target" argument must be one of type string, Buffer, or URL'
    }
  );
  common.expectsError(
    () => fs.symlink('', i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message:
        'The "path" argument must be one of type string, Buffer, or URL'
    }
  );
  common.expectsError(
    () => fs.symlinkSync(i, ''),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message:
        'The "target" argument must be one of type string, Buffer, or URL'
    }
  );
  common.expectsError(
    () => fs.symlinkSync('', i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message:
        'The "path" argument must be one of type string, Buffer, or URL'
    }
  );
});

common.expectsError(
  () => fs.symlink('', '', '🍏', common.mustNotCall()),
  {
    code: 'ERR_FS_INVALID_SYMLINK_TYPE',
    type: Error,
    message:
      'Symlink type must be one of "dir", "file", or "junction". Received "🍏"'
  }
);
common.expectsError(
  () => fs.symlinkSync('', '', '🍏'),
  {
    code: 'ERR_FS_INVALID_SYMLINK_TYPE',
    type: Error,
    message:
      'Symlink type must be one of "dir", "file", or "junction". Received "🍏"'
  }
);

process.on('exit', function() {
  assert.notStrictEqual(linkTime, fileTime);
});
