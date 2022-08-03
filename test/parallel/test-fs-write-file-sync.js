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

if (!common.isMainThread)
  common.skip('Setting process.umask is not supported in Workers');

const assert = require('assert');
const path = require('path');
const fs = require('fs');

// On Windows chmod is only able to manipulate read-only bit. Test if creating
// the file in read-only mode works.
const mode = common.isWindows ? 0o444 : 0o755;

// Reset the umask for testing
process.umask(0o000);

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Test writeFileSync
{
  const file = path.join(tmpdir.path, 'testWriteFileSync.txt');

  fs.writeFileSync(file, '123', { mode });
  const content = fs.readFileSync(file, { encoding: 'utf8' });
  assert.strictEqual(content, '123');
  assert.strictEqual(fs.statSync(file).mode & 0o777, mode);
}

// Test appendFileSync
{
  const file = path.join(tmpdir.path, 'testAppendFileSync.txt');

  fs.appendFileSync(file, 'abc', { mode });
  const content = fs.readFileSync(file, { encoding: 'utf8' });
  assert.strictEqual(content, 'abc');
  assert.strictEqual(fs.statSync(file).mode & mode, mode);
}

// Test writeFileSync with file descriptor
{
  // Need to hijack fs.open/close to make sure that things
  // get closed once they're opened.
  const _openSync = fs.openSync;
  const _closeSync = fs.closeSync;
  let openCount = 0;

  fs.openSync = (...args) => {
    openCount++;
    return _openSync(...args);
  };

  fs.closeSync = (...args) => {
    openCount--;
    return _closeSync(...args);
  };

  const file = path.join(tmpdir.path, 'testWriteFileSyncFd.txt');
  const fd = fs.openSync(file, 'w+', mode);

  fs.writeFileSync(fd, '123');
  fs.closeSync(fd);
  const content = fs.readFileSync(file, { encoding: 'utf8' });
  assert.strictEqual(content, '123');
  assert.strictEqual(fs.statSync(file).mode & 0o777, mode);

  // Verify that all opened files were closed.
  assert.strictEqual(openCount, 0);
  fs.openSync = _openSync;
  fs.closeSync = _closeSync;
}

// Test writeFileSync with flags
{
  const file = path.join(tmpdir.path, 'testWriteFileSyncFlags.txt');

  fs.writeFileSync(file, 'hello ', { encoding: 'utf8', flag: 'a' });
  fs.writeFileSync(file, 'world!', { encoding: 'utf8', flag: 'a' });
  const content = fs.readFileSync(file, { encoding: 'utf8' });
  assert.strictEqual(content, 'hello world!');
}

// Test writeFileSync with an object with an own toString function
{
  // Runtime deprecated by DEP0162
  common.expectWarning('DeprecationWarning',
                       'Implicit coercion of objects with own toString property is deprecated.',
                       'DEP0162');
  const file = path.join(tmpdir.path, 'testWriteFileSyncStringify.txt');
  const data = {
    toString() {
      return 'hello world!';
    }
  };

  fs.writeFileSync(file, data, { encoding: 'utf8', flag: 'a' });
  const content = fs.readFileSync(file, { encoding: 'utf8' });
  assert.strictEqual(content, String(data));
}
