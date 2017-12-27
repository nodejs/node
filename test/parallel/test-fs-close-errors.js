'use strict';

// This tests that the errors thrown from fs.close and fs.closeSync
// include the desired properties

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const uv = process.binding('uv');

['', false, null, undefined, {}, []].forEach((i) => {
  common.expectsError(
    () => fs.close(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type integer'
    }
  );
  common.expectsError(
    () => fs.closeSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type integer'
    }
  );
});

{
  assert.throws(
    () => {
      const fd = fs.openSync(__filename, 'r');
      fs.closeSync(fd);
      fs.closeSync(fd);
    },
    (err) => {
      assert.strictEqual(err.code, 'EBADF');
      assert.strictEqual(
        err.message,
        'EBADF: bad file descriptor, close'
      );
      assert.strictEqual(err.constructor, Error);
      assert.strictEqual(err.syscall, 'close');
      assert.strictEqual(err.errno, uv.UV_EBADF);
      return true;
    }
  );
}

{
  const fd = fs.openSync(__filename, 'r');
  fs.close(fd, common.mustCall((err) => {
    assert.ifError(err);
    fs.close(fd, common.mustCall((err) => {
      assert.strictEqual(err.code, 'EBADF');
      assert.strictEqual(
        err.message,
        'EBADF: bad file descriptor, close'
      );
      assert.strictEqual(err.constructor, Error);
      assert.strictEqual(err.syscall, 'close');
      assert.strictEqual(err.errno, uv.UV_EBADF);
    }));
  }));
}
