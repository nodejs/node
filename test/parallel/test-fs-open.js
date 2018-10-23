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

let caughtException = false;

try {
  // should throw ENOENT, not EBADF
  // see https://github.com/joyent/node/pull/1228
  fs.openSync('/path/to/file/that/does/not/exist', 'r');
} catch (e) {
  assert.strictEqual(e.code, 'ENOENT');
  caughtException = true;
}
assert.strictEqual(caughtException, true);

fs.openSync(__filename);

fs.open(__filename, common.mustCall((err) => {
  assert.ifError(err);
}));

fs.open(__filename, 'r', common.mustCall((err) => {
  assert.ifError(err);
}));

fs.open(__filename, 'rs', common.mustCall((err) => {
  assert.ifError(err);
}));

fs.open(__filename, 'r', 0, common.mustCall((err) => {
  assert.ifError(err);
}));

fs.open(__filename, 'r', null, common.mustCall((err) => {
  assert.ifError(err);
}));

async function promise() {
  await fs.promises.open(__filename);
  await fs.promises.open(__filename, 'r');
}

promise().then(common.mustCall()).catch(common.mustNotCall());

common.expectsError(
  () => fs.open(__filename, 'r', 'boom', common.mustNotCall()),
  {
    code: 'ERR_INVALID_ARG_VALUE',
    type: TypeError
  }
);

for (const extra of [[], ['r'], ['r', 0], ['r', 0, 'bad callback']]) {
  common.expectsError(
    () => fs.open(__filename, ...extra),
    {
      code: 'ERR_INVALID_CALLBACK',
      type: TypeError
    }
  );
}

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
  fs.promises.open(i, 'r')
    .then(common.mustNotCall())
    .catch(common.mustCall((err) => {
      common.expectsError(
        () => { throw err; },
        {
          code: 'ERR_INVALID_ARG_TYPE',
          type: TypeError
        }
      );
    }));
});
