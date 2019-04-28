'use strict';

const fs = require('fs');
const path = require('path');
const assert = require('assert');
const shutil = require('shutil');

const tmpdir = require('../common/tmpdir');

const tmpDir = tmpdir.path;
const baseDir = path.join(tmpDir, 'shutil-rmtree');

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
  assert.throws(
    () => fs.readdirSync(path),
    {
      code: 'ENOENT'
    }
  );
}

function test_rmtree_sync(path) {
  setupFiles();

  assert(fs.readdirSync(path));

  shutil.rmtreeSync(path);
  assertGone(path);
}


function test_rmtree(path) {
  setupFiles();

  assert(fs.readdirSync(path));

  shutil.rmtree(path, () => assertGone(path));
}

test_rmtree_sync(baseDir);
test_rmtree(baseDir);
