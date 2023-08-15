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
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

const fs = require('fs');

const fileFixture = fixtures.path('a.js');
const fileTemp = tmpdir.resolve('a.js');

// Copy fixtures to temp.
tmpdir.refresh();
fs.copyFileSync(fileFixture, fileTemp);

fs.open(fileTemp, 'a', 0o777, common.mustSucceed((fd) => {
  fs.fdatasyncSync(fd);

  fs.fsyncSync(fd);

  fs.fdatasync(fd, common.mustSucceed(() => {
    fs.fsync(fd, common.mustSucceed(() => {
      fs.closeSync(fd);
    }));
  }));
}));

['', false, null, undefined, {}, []].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError'
  };
  assert.throws(() => fs.fdatasync(input), errObj);
  assert.throws(() => fs.fdatasyncSync(input), errObj);
  assert.throws(() => fs.fsync(input), errObj);
  assert.throws(() => fs.fsyncSync(input), errObj);
});
