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
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

let dirc = 0;
function nextdir() {
  return `test${++dirc}`;
}

// mkdir creates directory using assigned path
{
  const pathname = path.join(tmpdir.path, nextdir());

  fs.mkdir(pathname, common.mustCall(function(err) {
    assert.strictEqual(err, null);
    assert.strictEqual(fs.existsSync(pathname), true);
  }));
}

// mkdir creates directory with assigned mode value
{
  const pathname = path.join(tmpdir.path, nextdir());

  fs.mkdir(pathname, 0o777, common.mustCall(function(err) {
    assert.strictEqual(err, null);
    assert.strictEqual(fs.existsSync(pathname), true);
  }));
}

// mkdirSync successfully creates directory from given path
{
  const pathname = path.join(tmpdir.path, nextdir());

  fs.mkdirSync(pathname);

  const exists = fs.existsSync(pathname);
  assert.strictEqual(exists, true);
}

// mkdirSync and mkdir require path to be a string, buffer or url.
// Anything else generates an error.
[false, 1, {}, [], null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.mkdir(i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.mkdirSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});

// mkdirpSync when both top-level, and sub-folders do not exist.
{
  const pathname = path.join(tmpdir.path, nextdir(), nextdir());

  fs.mkdirSync(pathname, { recursive: true });

  const exists = fs.existsSync(pathname);
  assert.strictEqual(exists, true);
  assert.strictEqual(fs.statSync(pathname).isDirectory(), true);
}

// mkdirpSync when folder already exists.
{
  const pathname = path.join(tmpdir.path, nextdir(), nextdir());

  fs.mkdirSync(pathname, { recursive: true });
  // should not cause an error.
  fs.mkdirSync(pathname, { recursive: true });

  const exists = fs.existsSync(pathname);
  assert.strictEqual(exists, true);
  assert.strictEqual(fs.statSync(pathname).isDirectory(), true);
}

// mkdirpSync ../
{
  const pathname = `${tmpdir.path}/${nextdir()}/../${nextdir()}/${nextdir()}`;
  fs.mkdirSync(pathname, { recursive: true });
  const exists = fs.existsSync(pathname);
  assert.strictEqual(exists, true);
  assert.strictEqual(fs.statSync(pathname).isDirectory(), true);
}

// mkdirpSync when path is a file.
{
  const pathname = path.join(tmpdir.path, nextdir(), nextdir());

  fs.mkdirSync(path.dirname(pathname));
  fs.writeFileSync(pathname, '', 'utf8');

  try {
    fs.mkdirSync(pathname, { recursive: true });
    throw new Error('unreachable');
  } catch (err) {
    assert.notStrictEqual(err.message, 'unreachable');
    assert.strictEqual(err.code, 'EEXIST');
    assert.strictEqual(err.syscall, 'mkdir');
  }
}

// mkdirp when folder does not yet exist.
{
  const pathname = path.join(tmpdir.path, nextdir(), nextdir());

  fs.mkdir(pathname, { recursive: true }, common.mustCall(function(err) {
    assert.strictEqual(err, null);
    assert.strictEqual(fs.existsSync(pathname), true);
    assert.strictEqual(fs.statSync(pathname).isDirectory(), true);
  }));
}

// mkdirp when path is a file.
{
  const pathname = path.join(tmpdir.path, nextdir(), nextdir());

  fs.mkdirSync(path.dirname(pathname));
  fs.writeFileSync(pathname, '', 'utf8');
  fs.mkdir(pathname, { recursive: true }, (err) => {
    assert.strictEqual(err.code, 'EEXIST');
    assert.strictEqual(err.syscall, 'mkdir');
    assert.strictEqual(fs.statSync(pathname).isDirectory(), false);
  });
}

// mkdirpSync dirname loop
// XXX: windows and smartos have issues removing a directory that you're in.
if (common.isMainThread && (common.isLinux || common.isOSX)) {
  const pathname = path.join(tmpdir.path, nextdir());
  fs.mkdirSync(pathname);
  process.chdir(pathname);
  fs.rmdirSync(pathname);
  try {
    fs.mkdirSync('X', { recursive: true });
    throw new Error('unreachable');
  } catch (err) {
    assert.notStrictEqual(err.message, 'unreachable');
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'mkdir');
  }
  fs.mkdir('X', { recursive: true }, (err) => {
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'mkdir');
  });
}

// mkdirSync and mkdir require options.recursive to be a boolean.
// Anything else generates an error.
{
  const pathname = path.join(tmpdir.path, nextdir());
  ['', 1, {}, [], null, Symbol('test'), () => {}].forEach((recursive) => {
    common.expectsError(
      () => fs.mkdir(pathname, { recursive }, common.mustNotCall()),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "recursive" argument must be of type boolean. Received ' +
          `type ${typeof recursive}`
      }
    );
    common.expectsError(
      () => fs.mkdirSync(pathname, { recursive }),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "recursive" argument must be of type boolean. Received ' +
          `type ${typeof recursive}`
      }
    );
  });
}

// Keep the event loop alive so the async mkdir() requests
// have a chance to run (since they don't ref the event loop).
process.nextTick(() => {});
