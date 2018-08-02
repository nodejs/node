'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const d = path.join(tmpdir.path, 'dir');
const f = path.join(d, 'test.bin');

{
  tmpdir.refresh();
  // Make sure the directory does not exist
  assert(!common.fileExists(d));
  // Create the directory now
  fs.mkdirSync(d);
  // Make sure the directory exists
  assert(common.fileExists(d));
  // Try creating again, it should fail with EEXIST
  assert.throws(function() {
    fs.mkdirSync(d);
  }, /EEXIST: file already exists, mkdir/);
  // Remove the directory now
  fs.rmdirSync(d);
  // Make sure the directory does not exist
  assert(!common.fileExists(d));
}

{
  tmpdir.refresh();
  // Create the directory
  assert(!common.fileExists(d));
  fs.mkdirSync(d);
  assert(common.fileExists(d));
  // Create the file
  fs.writeFileSync(f, 'foo'.repeat(2 ** 15));
  // Make sure the file exists
  assert(common.fileExists(f));
  // Try removing a file that exists, ...
  // in POSIX systems: it should fail with ENOTDIR
  // in Window systems: it should fail with ENOENT
  if (common.isWindows) {
    assert.throws(function() {
      fs.rmdirSync(f);
    }, /ENOENT: not a directory, rmdir/);
  } else {
    assert.throws(function() {
      fs.rmdirSync(f);
    }, /ENOTDIR: not a directory, rmdir/);
  }
}

{
  tmpdir.refresh();
  // Similarly test the Async version
  fs.mkdir(d, 0o666, common.mustCall(function(err) {
    assert.ifError(err);

    fs.mkdir(d, 0o666, common.mustCall(function(err) {
      assert.strictEqual(this, undefined);
      assert.ok(err, 'got no error');
      assert.ok(/^EEXIST/.test(err.message), 'got no EEXIST message');
      assert.strictEqual(err.code, 'EEXIST');
      assert.strictEqual(err.path, d);

      fs.rmdir(d, assert.ifError);
    }));
  }));
}
