'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');

const fs = require('fs');
const path = require('path');
const assert = require('assert');

const tmpDir = tmpdir.path;
const baseDir = path.join(tmpDir, 'fs-rmtree');

function setupFiles() {
  fs.mkdirSync(baseDir);
  fs.mkdirSync(path.join(baseDir, 'dir1'));
  fs.mkdirSync(path.join(baseDir, 'dir2'));
  fs.mkdirSync(path.join(baseDir, 'dir2', 'dir3'));
  fs.mkdirSync(path.join(baseDir, 'dir4'));

  fs.closeSync(fs.openSync(path.join(baseDir, 'dir1', 'file1'), 'w'));
  fs.closeSync(fs.openSync(path.join(baseDir, 'dir1', 'file2'), 'w'));
  fs.closeSync(fs.openSync(path.join(baseDir, 'dir2', 'file3'), 'w'));
  fs.closeSync(fs.openSync(path.join(baseDir, 'dir2', 'dir3', 'file4'), 'w'));
  fs.closeSync(fs.openSync(path.join(baseDir, 'dir2', 'dir3', 'file5'), 'w'));
}

function assertGone(path) {
  assert.throws(() => fs.readdirSync(path), {
    code: 'ENOENT'
  });
}

function test_rmtree_sync(path) {
  setupFiles();

  assert(fs.readdirSync(path));

  fs.rmtreeSync(path);

  assertGone(path);
}

function test_invalid_path() {
  assert.throws(() => fs.rmtreeSync(1), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
}

function test_invalid_callback(path) {
  assert.throws(() => fs.rmtree(path, {}, 1), {
    code: 'ERR_INVALID_CALLBACK'
  });
}

function test_rmtree(path) {
  setupFiles();

  assert(fs.readdirSync(path));

  fs.rmtree(path, () => assertGone(path));
}

test_rmtree_sync(baseDir);
test_invalid_path();
test_invalid_callback(baseDir);
test_rmtree(baseDir);
