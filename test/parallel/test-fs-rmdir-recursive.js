// Flags: --expose-internals
'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { validateRmdirOptions } = require('internal/fs/utils');
let count = 0;

tmpdir.refresh();

function makeNonEmptyDirectory() {
  const dirname = `rmdir-recursive-${count}`;
  fs.mkdirSync(path.join(dirname, 'foo', 'bar', 'baz'), { recursive: true });
  fs.writeFileSync(path.join(dirname, 'text.txt'), 'hello', 'utf8');
  count++;
  return dirname;
}

// Test the asynchronous version.
{
  const dir = makeNonEmptyDirectory();

  // Removal should fail without the recursive option.
  fs.rmdir(dir, common.mustCall((err) => {
    assert.strictEqual(err.syscall, 'rmdir');

    // Removal should fail without the recursive option set to true.
    fs.rmdir(dir, { recursive: false }, common.mustCall((err) => {
      assert.strictEqual(err.syscall, 'rmdir');

      // Recursive removal should succeed.
      fs.rmdir(dir, { recursive: true }, common.mustCall((err) => {
        assert.ifError(err);

        // No error should occur if recursive and the directory does not exist.
        fs.rmdir(dir, { recursive: true }, common.mustCall((err) => {
          assert.ifError(err);

          // Attempted removal should fail now because the directory is gone.
          fs.rmdir(dir, common.mustCall((err) => {
            assert.strictEqual(err.syscall, 'rmdir');
          }));
        }));
      }));
    }));
  }));
}

// Test the synchronous version.
{
  const dir = makeNonEmptyDirectory();

  // Removal should fail without the recursive option set to true.
  common.expectsError(() => {
    fs.rmdirSync(dir);
  }, { syscall: 'rmdir' });
  common.expectsError(() => {
    fs.rmdirSync(dir, { recursive: false });
  }, { syscall: 'rmdir' });

  // Recursive removal should succeed.
  fs.rmdirSync(dir, { recursive: true });

  // No error should occur if recursive and the directory does not exist.
  fs.rmdirSync(dir, { recursive: true });

  // Attempted removal should fail now because the directory is gone.
  common.expectsError(() => fs.rmdirSync(dir), { syscall: 'rmdir' });
}

// Test the Promises based version.
(async () => {
  const dir = makeNonEmptyDirectory();

  // Removal should fail without the recursive option set to true.
  assert.rejects(fs.promises.rmdir(dir), { syscall: 'rmdir' });
  assert.rejects(fs.promises.rmdir(dir, { recursive: false }), {
    syscall: 'rmdir'
  });

  // Recursive removal should succeed.
  await fs.promises.rmdir(dir, { recursive: true });

  // No error should occur if recursive and the directory does not exist.
  await fs.promises.rmdir(dir, { recursive: true });

  // Attempted removal should fail now because the directory is gone.
  assert.rejects(fs.promises.rmdir(dir), { syscall: 'rmdir' });
})();

// Test input validation.
{
  const defaults = {
    emfileWait: 1000,
    maxBusyTries: 3,
    recursive: false
  };
  const modified = {
    emfileWait: 953,
    maxBusyTries: 5,
    recursive: true
  };

  assert.deepStrictEqual(validateRmdirOptions(), defaults);
  assert.deepStrictEqual(validateRmdirOptions({}), defaults);
  assert.deepStrictEqual(validateRmdirOptions(modified), modified);
  assert.deepStrictEqual(validateRmdirOptions({
    maxBusyTries: 99
  }), {
    emfileWait: 1000,
    maxBusyTries: 99,
    recursive: false
  });

  [null, 'foo', 5, NaN].forEach((bad) => {
    common.expectsError(() => {
      validateRmdirOptions(bad);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: /^The "options" argument must be of type object\./
    });
  });

  [undefined, null, 'foo', Infinity, function() {}].forEach((bad) => {
    common.expectsError(() => {
      validateRmdirOptions({ recursive: bad });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: /^The "recursive" argument must be of type boolean\./
    });
  });

  common.expectsError(() => {
    validateRmdirOptions({ emfileWait: -1 });
  }, {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: /^The value of "emfileWait" is out of range\./
  });

  common.expectsError(() => {
    validateRmdirOptions({ maxBusyTries: -1 });
  }, {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: /^The value of "maxBusyTries" is out of range\./
  });
}
