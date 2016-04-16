'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var path = require('path');
var doesNotExist = path.join(common.tmpDir, '__this_should_not_exist');
var readOnlyFile = path.join(common.tmpDir, 'read_only_file');
var readWriteFile = path.join(common.tmpDir, 'read_write_file');

var removeFile = function(file) {
  try {
    fs.unlinkSync(file);
  } catch (err) {
    // Ignore error
  }
};

var createFileWithPerms = function(file, mode) {
  removeFile(file);
  fs.writeFileSync(file, '');
  fs.chmodSync(file, mode);
};

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
 * priviledges to create them.
 *
 * There's not really any point in resetting the process' user id to 0 after
 * changing it to 'nobody', since in the case that the test runs without
 * superuser priviledge, it is not possible to change its process user id to
 * superuser.
 *
 * It can prevent the test from removing files created before the change of user
 * id, but that's fine. In this case, it is the responsability of the
 * continuous integration platform to take care of that.
 */
var hasWriteAccessForReadonlyFile = false;
if (!common.isWindows && process.getuid() === 0) {
  hasWriteAccessForReadonlyFile = true;
  try {
    process.setuid('nobody');
    hasWriteAccessForReadonlyFile = false;
  } catch (err) {
  }
}

assert(typeof fs.F_OK === 'number');
assert(typeof fs.R_OK === 'number');
assert(typeof fs.W_OK === 'number');
assert(typeof fs.X_OK === 'number');

fs.access(__filename, function(err) {
  assert.strictEqual(err, null, 'error should not exist');
});

fs.access(__filename, fs.R_OK, function(err) {
  assert.strictEqual(err, null, 'error should not exist');
});

fs.access(doesNotExist, function(err) {
  assert.notEqual(err, null, 'error should exist');
  assert.strictEqual(err.code, 'ENOENT');
  assert.strictEqual(err.path, doesNotExist);
});

fs.access(readOnlyFile, fs.F_OK | fs.R_OK, function(err) {
  assert.strictEqual(err, null, 'error should not exist');
});

fs.access(readOnlyFile, fs.W_OK, function(err) {
  if (hasWriteAccessForReadonlyFile) {
    assert.equal(err, null, 'error should not exist');
  } else {
    assert.notEqual(err, null, 'error should exist');
    assert.strictEqual(err.path, readOnlyFile);
  }
});

assert.throws(function() {
  fs.access(100, fs.F_OK, function(err) {});
}, /path must be a string or Buffer/);

assert.throws(function() {
  fs.access(__filename, fs.F_OK);
}, /"callback" argument must be a function/);

assert.throws(function() {
  fs.access(__filename, fs.F_OK, {});
}, /"callback" argument must be a function/);

assert.doesNotThrow(function() {
  fs.accessSync(__filename);
});

assert.doesNotThrow(function() {
  var mode = fs.F_OK | fs.R_OK | fs.W_OK;

  fs.accessSync(readWriteFile, mode);
});

assert.throws(function() {
  fs.accessSync(doesNotExist);
}, function(err) {
  return err.code === 'ENOENT' && err.path === doesNotExist;
});

process.on('exit', function() {
  removeFile(readOnlyFile);
  removeFile(readWriteFile);
});
