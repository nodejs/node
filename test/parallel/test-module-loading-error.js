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

const error_desc = {
  win32: ['%1 is not a valid Win32 application'],
  linux: ['file too short', 'Exec format error'],
  sunos: ['unknown file type', 'not an ELF file'],
  darwin: ['file too short']
};
const dlerror_msg = error_desc[process.platform];

assert.throws(
  () => { require('../fixtures/module-loading-error.node'); },
  (e) => {
    if (dlerror_msg && !dlerror_msg.some((msg) => e.message.includes(msg)))
      return false;
    if (e.name !== 'Error')
      return false;
    return true;
  }
);

assert.throws(
  require,
  /^AssertionError: missing path$/
);

assert.throws(
  () => { require({}); },
  /^AssertionError: path must be a string$/
);
