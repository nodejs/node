'use strict';

// This tests that fs.access and fs.accessSync works as expected
// and the errors thrown from these APIs include the desired properties

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const uv = process.binding('uv');

const doesNotExist = path.join(common.tmpDir, '__this_should_not_exist');
const readOnlyFile = path.join(common.tmpDir, 'read_only_file');
const readWriteFile = path.join(common.tmpDir, 'read_write_file');

function createFileWithPerms(file, mode) {
  fs.writeFileSync(file, '');
  fs.chmodSync(file, mode);
}

common.refreshTmpDir();
createFileWithPerms(readOnlyFile, 0o444);
createFileWithPerms(readWriteFile, 0o666);

/*
 * On non-Windows supported platforms, fs.access(readOnlyFile, W_OK, ...)
 * always succeeds if node runs as the super user, which is sometimes the
 * case for tests running on our continuous testing platform agents.
 *
 * In this case, this test tries to change its process user id to a
 * non-superuser user so that the test that checks for write access to a
 * read-only file can be more meaningful.
 *
 * The change of user id is done after creating the fixtures files for the same
 * reason: the test may be run as the superuser within a directory in which
 * only the superuser can create files, and thus it may need superuser
 * privileges to create them.
 *
 * There's not really any point in resetting the process' user id to 0 after
 * changing it to 'nobody', since in the case that the test runs without
 * superuser privilege, it is not possible to change its process user id to
 * superuser.
 *
 * It can prevent the test from removing files created before the change of user
 * id, but that's fine. In this case, it is the responsibility of the
 * continuous integration platform to take care of that.
 */
let hasWriteAccessForReadonlyFile = false;
if (!common.isWindows && process.getuid() === 0) {
  hasWriteAccessForReadonlyFile = true;
  try {
    process.setuid('nobody');
    hasWriteAccessForReadonlyFile = false;
  } catch (err) {
  }
}

assert.strictEqual(typeof fs.F_OK, 'number');
assert.strictEqual(typeof fs.R_OK, 'number');
assert.strictEqual(typeof fs.W_OK, 'number');
assert.strictEqual(typeof fs.X_OK, 'number');

fs.access(__filename, common.mustCall((err) => {
  assert.ifError(err);
}));

fs.access(__filename, fs.R_OK, common.mustCall((err) => {
  assert.ifError(err);
}));

fs.access(doesNotExist, common.mustCall((err) => {
  assert.notStrictEqual(err, null, 'error should exist');
  assert.strictEqual(err.code, 'ENOENT');
  assert.strictEqual(err.path, doesNotExist);
}));

fs.access(readOnlyFile, fs.F_OK | fs.R_OK, common.mustCall((err) => {
  assert.ifError(err);
}));

fs.access(readOnlyFile, fs.W_OK, common.mustCall((err) => {
  if (hasWriteAccessForReadonlyFile) {
    assert.ifError(err);
  } else {
    assert.notStrictEqual(err, null, 'error should exist');
    assert.strictEqual(err.path, readOnlyFile);
  }
}));

common.expectsError(
  () => {
    fs.access(100, fs.F_OK, common.mustNotCall());
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "path" argument must be one of type string, Buffer, or URL'
  }
);

common.expectsError(
  () => {
    fs.access(__filename, fs.F_OK);
  },
  {
    code: 'ERR_INVALID_CALLBACK',
    type: TypeError
  });

common.expectsError(
  () => {
    fs.access(__filename, fs.F_OK, {});
  },
  {
    code: 'ERR_INVALID_CALLBACK',
    type: TypeError
  });

assert.doesNotThrow(() => {
  fs.accessSync(__filename);
});

assert.doesNotThrow(() => {
  const mode = fs.F_OK | fs.R_OK | fs.W_OK;

  fs.accessSync(readWriteFile, mode);
});

assert.throws(
  () => { fs.accessSync(doesNotExist); },
  (err) => {
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.path, doesNotExist);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, access '${doesNotExist}'`
    );
    assert.strictEqual(err.constructor, Error);
    assert.strictEqual(err.syscall, 'access');
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    return true;
  }
);

assert.throws(
  () => { fs.accessSync(Buffer.from(doesNotExist)); },
  (err) => {
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.path, doesNotExist);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, access '${doesNotExist}'`
    );
    assert.strictEqual(err.constructor, Error);
    assert.strictEqual(err.syscall, 'access');
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    return true;
  }
);
