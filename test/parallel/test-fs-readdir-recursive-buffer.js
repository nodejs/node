'use strict';

// Regression test for https://github.com/nodejs/node/issues/58892
// `readdir`/`readdirSync` with `{ recursive: true }` throw
// ERR_INVALID_ARG_TYPE when `encoding: 'buffer'` is used, because the
// internal recursive walk joins path segments with `path.join()`, which
// does not accept Buffer arguments.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const nested = path.join(tmpdir.path, 'a', 'b');
fs.mkdirSync(nested, { recursive: true });
fs.writeFileSync(path.join(nested, 'file.txt'), 'hello');

// readdirSync
const syncResult = fs.readdirSync(tmpdir.path, { recursive: true, encoding: 'buffer' });
assert.ok(syncResult.every((entry) => Buffer.isBuffer(entry)));
assert.ok(syncResult.some((entry) => entry.toString().includes('file.txt')));

// readdirSync with withFileTypes
const syncDirents = fs.readdirSync(
  tmpdir.path,
  { recursive: true, encoding: 'buffer', withFileTypes: true }
);
assert.ok(syncDirents.some((dirent) => dirent.name.toString() === 'file.txt'));

// readdir (callback)
fs.readdir(
  tmpdir.path,
  { recursive: true, encoding: 'buffer' },
  common.mustSucceed((entries) => {
    assert.ok(entries.every((entry) => Buffer.isBuffer(entry)));
    assert.ok(entries.some((entry) => entry.toString().includes('file.txt')));
  })
);

// fs.promises.readdir
fs.promises
  .readdir(tmpdir.path, { recursive: true, encoding: 'buffer' })
  .then(common.mustCall((entries) => {
    assert.ok(entries.every((entry) => Buffer.isBuffer(entry)));
    assert.ok(entries.some((entry) => entry.toString().includes('file.txt')));
  }));
