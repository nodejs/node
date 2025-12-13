'use strict';

// Test that readdir callback works with Buffer paths in recursive mode.
// Refs: https://github.com/nodejs/node/issues/58892

const common = require('../common');
const tmpdir = require('../common/tmpdir');

const assert = require('node:assert');
const { readdir, mkdirSync, writeFileSync } = require('node:fs');
const { join } = require('node:path');

tmpdir.refresh();

const subdir = join(tmpdir.path, 'subdir');
mkdirSync(subdir);
writeFileSync(join(tmpdir.path, 'file1.txt'), 'content1');
writeFileSync(join(subdir, 'file2.txt'), 'content2');

readdir(Buffer.from(tmpdir.path), { recursive: true }, common.mustSucceed((result) => {
  assert(Array.isArray(result));
  assert.strictEqual(result.length, 3);
}));

readdir(Buffer.from(tmpdir.path), { recursive: true, withFileTypes: true }, common.mustSucceed((result) => {
  assert(Array.isArray(result));
  assert.strictEqual(result.length, 3);
}));
