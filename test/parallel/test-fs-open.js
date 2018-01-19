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

common.crashOnUnhandledRejection();

common.expectsError(
  () => fs.openSync('/path/to/file/that/does/not/exist', 'r'),
  {
    code: 'ENOENT',
    type: Error
  }
);

common.expectsError(
  () => fs.openFileHandleSync('/path/to/file/that/does/not/exist', 'r'),
  {
    code: 'ENOENT',
    type: Error
  }
);

fs.open(__filename, 'r', common.mustCall((err, fd) => {
  assert.ifError(err);
  assert.strictEqual(typeof fd, 'number');
}));

fs.open(__filename, 'rs', common.mustCall((err, fd) => {
  assert.ifError(err);
  assert.strictEqual(typeof fd, 'number');
}));

fs.openFileHandle(__filename, 'r', common.mustCall((err, fd) => {
  assert.ifError(err);
  assert.strictEqual(typeof fd, 'object');
  assert.strictEqual(typeof fd.fd, 'number');
  fd.close().then(common.mustCall()).catch(common.mustNotCall());
  fd.close().then(common.mustNotCall()).catch(common.mustCall());
}));

fs.openFileHandle(__filename, 'rs', common.mustCall((err, fd) => {
  assert.ifError(err);
  assert.strictEqual(typeof fd, 'object');
  assert.strictEqual(typeof fd.fd, 'number');
  fd.close().then(common.mustCall()).catch(common.mustNotCall());
  fd.close().then(common.mustNotCall()).catch(common.mustCall());
}));

[false, 1, [], {}, null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.open(i, 'r', common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.openSync(i, 'r', common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.openFileHandle(i, 'r', common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.openFileHandleSync(i, 'r', common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});
