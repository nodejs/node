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


// Copied from /test/parallel/test-fs-open.js using options-signature
(function() {
  const common = require('../common');
  const assert = require('assert');
  const fs = require('fs');

  let caughtException = false;

  try {
    // Should throw ENOENT, not EBADF
    // see https://github.com/joyent/node/pull/1228
    fs.openSync('/8hvftyuncxrt/path/to/file/that/does/not/exist', {
      flag: 'r'
    });
  } catch (e) {
    assert.strictEqual(e.code, 'ENOENT');
    caughtException = true;
  }
  assert.strictEqual(caughtException, true);

  fs.openSync(__filename);

  fs.open(__filename, common.mustSucceed());

  fs.open(__filename, { flag: 'r' }, common.mustSucceed());

  fs.open(__filename, { flag: 'rs' }, common.mustSucceed());

  fs.open(__filename, { flag: 'r', mode: 0 }, common.mustSucceed());

  fs.open(__filename, { flag: 'r', mode: null }, common.mustSucceed());

  async function promise() {
    await fs.promises.open(__filename);
    await fs.promises.open(__filename, { flag: 'r' });
  }

  promise().then(common.mustCall()).catch(common.mustNotCall());

  assert.throws(
    () => fs.open(__filename, {
      flag: 'r',
      mode: 'boom'
    }, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError'
    }
  );

  for (const extra of [[], ['r'], ['r', 0], ['r', 0, 'bad callback']]) {
    assert.throws(
      () => fs.open(__filename, ...extra),
      {
        code: 'ERR_INVALID_CALLBACK',
        name: 'TypeError'
      }
    );
  }

  [false, 1, [], {}, null, undefined].forEach((i) => {
    assert.throws(
      () => fs.open(i, { flag: 'r' }, common.mustNotCall()),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError'
      }
    );
    assert.throws(
      () => fs.openSync(i, { flag: 'r' }, common.mustNotCall()),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError'
      }
    );
    assert.rejects(
      fs.promises.open(i, { flag: 'r' }),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError'
      }
    );
  });

  // Check invalid modes.
  [false, [], {}].forEach((mode) => {
    assert.throws(
      () => fs.open(__filename, { flag: 'r', mode }, common.mustNotCall()),
      {
        message: /'mode' must be a 32-bit/,
        code: 'ERR_INVALID_ARG_VALUE'
      }
    );
    assert.throws(
      () => fs.openSync(__filename, { flag: 'r', mode }, common.mustNotCall()),
      {
        message: /'mode' must be a 32-bit/,
        code: 'ERR_INVALID_ARG_VALUE'
      }
    );
    assert.rejects(
      fs.promises.open(__filename, { flag: 'r', mode }),
      {
        message: /'mode' must be a 32-bit/,
        code: 'ERR_INVALID_ARG_VALUE'
      }
    );
  });
}());


// Copied from /test/parallel/test-fs-open-flags.js using options-signature
(function() {
  // Flags: --expose-internals
  const common = require('../common');

  const fixtures = require('../common/fixtures');

  // const assert = require('assert');
  const fs = require('fs');
  const path = require('path');

  if (common.isLinux || common.isOSX) {
    const tmpdir = require('../common/tmpdir');
    tmpdir.refresh();
    const file = path.join(tmpdir.path, 'a.js');
    fs.copyFileSync(fixtures.path('a.js'), file);
    fs.open(file, { flag: fs.constants.O_DSYNC }, common.mustSucceed((fd) => {
      fs.closeSync(fd);
    }));
  }
}());


// Copied from /test/parallel/test-fs-open-mode-mask.js using options-signature
(function() {
  // This tests that the lower bits of mode > 0o777 still works in fs.open().

  const common = require('../common');
  const assert = require('assert');
  const path = require('path');
  const fs = require('fs');

  const mode = common.isWindows ? 0o444 : 0o644;

  const maskToIgnore = 0o10000;

  const tmpdir = require('../common/tmpdir');
  // tmpdir.refresh();

  function test(mode, asString) {
    const suffix = asString ? 'str' : 'num';
    const input = asString ?
      (mode | maskToIgnore).toString(8) : (mode | maskToIgnore);

    {
      const file = path.join(tmpdir.path, `openSync-${suffix}.txt`);
      const fd = fs.openSync(file, { flag: 'w+', mask: input });
      assert.strictEqual(fs.fstatSync(fd).mode & 0o777, mode);
      fs.closeSync(fd);
      assert.strictEqual(fs.statSync(file).mode & 0o777, mode);
    }

    {
      const file = path.join(tmpdir.path, `open-${suffix}.txt`);
      fs.open(file, { flag: 'w+', mask: input }, common.mustSucceed((fd) => {
        assert.strictEqual(fs.fstatSync(fd).mode & 0o777, mode);
        fs.closeSync(fd);
        assert.strictEqual(fs.statSync(file).mode & 0o777, mode);
      }));
    }
  }

  test(mode, true);
  test(mode, false);
}());


// Copied from /test/parallel/test-fs-open-no-close.js using options-signature
(function() {
  // Refs: https://github.com/nodejs/node/issues/34266
  // Failing to close a file should not keep the event loop open.

  const common = require('../common');
  const assert = require('assert');

  const fs = require('fs');

  const debuglog = (arg) => {
    console.log(new Date().toLocaleString(), arg);
  };

  const tmpdir = require('../common/tmpdir');
  // tmpdir.refresh();

  {
    fs.open(`${tmpdir.path}/dummy`, {
      flag: 'wx+'
    }, common.mustCall((err, fd) => {
      debuglog('fs open() callback');
      assert.ifError(err);
    }));
    debuglog('waiting for callback');
  }
}());


// Copied from /test/parallel/test-fs-open-numeric-flags.js
// using options-signature
(function() {
  require('../common');

  const assert = require('assert');
  const fs = require('fs');
  const path = require('path');

  const tmpdir = require('../common/tmpdir');
  // tmpdir.refresh();

  // O_WRONLY without O_CREAT shall fail with ENOENT
  const pathNE = path.join(tmpdir.path, 'file-should-not-exist');
  assert.throws(
    () => fs.openSync(pathNE, { flag: fs.constants.O_WRONLY }),
    (e) => e.code === 'ENOENT'
  );
}());
