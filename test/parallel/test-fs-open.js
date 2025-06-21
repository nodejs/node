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
  // Should throw ENOENT, not EBADF
  // see https://github.com/joyent/node/pull/1228
  fs.openSync('/8hvftyuncxrt/path/to/file/that/does/not/exist', 'r');
} catch (e) {
  assert.strictEqual(e.code, 'ENOENT');
  caughtException = true;
}
assert.strictEqual(caughtException, true);

fs.openSync(__filename);

fs.open(__filename, common.mustSucceed());

fs.open(__filename, 'r', common.mustSucceed());

fs.open(__filename, 'rs', common.mustSucceed());

fs.open(__filename, 'r', 0, common.mustSucceed());

fs.open(__filename, 'r', null, common.mustSucceed());

async function promise() {
  await (await fs.promises.open(__filename)).close();
  await (await fs.promises.open(__filename, 'r')).close();
}

promise().then(common.mustCall()).catch(common.mustNotCall());

assert.throws(
  () => fs.open(__filename, 'r', 'boom', common.mustNotCall()),
  {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError'
  }
);

for (const extra of [[], ['r'], ['r', 0], ['r', 0, 'bad callback']]) {
  assert.throws(
    () => fs.open(__filename, ...extra),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
}

[false, 1, [], {}, null, undefined].forEach((i) => {
  assert.throws(
    () => fs.open(i, 'r', common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => fs.openSync(i, 'r', common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.rejects(
    fs.promises.open(i, 'r'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  ).then(common.mustCall());
});

// Check invalid modes.
[false, [], {}].forEach((mode) => {
  assert.throws(
    () => fs.open(__filename, 'r', mode, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE'
    }
  );
  assert.throws(
    () => fs.openSync(__filename, 'r', mode, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE'
    }
  );
  assert.rejects(
    fs.promises.open(__filename, 'r', mode),
    {
      code: 'ERR_INVALID_ARG_TYPE'
    }
  ).then(common.mustCall());
});
