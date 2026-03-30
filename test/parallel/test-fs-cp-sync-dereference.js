'use strict';

// Refs: https://github.com/nodejs/node/issues/58939
//
// In this test, both the cp and cpSync functions are attempting to copy
// a file over a symlinked directory.

const common = require('../common');

const {
  cp,
  cpSync,
  mkdirSync,
  symlinkSync,
  writeFileSync,
  readFileSync,
  statSync
} = require('fs');

const {
  join,
} = require('path');

const assert = require('assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const pathA = join(tmpdir.path, 'a'); // file
const pathB = join(tmpdir.path, 'b'); // directory
const patch = join(tmpdir.path, 'c'); // c -> b
const pathD = join(tmpdir.path, 'd'); // d -> b

writeFileSync(pathA, 'file a');
mkdirSync(pathB);
symlinkSync(pathB, patch, 'dir');
symlinkSync(pathB, pathD, 'dir');

cp(pathA, pathD, { dereference: false }, common.mustSucceed(() => {
  // The path d is now a file, not a symlink
  assert.strictEqual(readFileSync(pathA, 'utf-8'), readFileSync(pathD, 'utf-8'));
  assert.ok(statSync(pathA).isFile());
  assert.ok(statSync(pathD).isFile());
}));

cpSync(pathA, patch, { dereference: false });

assert.strictEqual(readFileSync(pathA, 'utf-8'), readFileSync(patch, 'utf-8'));
assert.ok(statSync(pathA).isFile());
assert.ok(statSync(patch).isFile());
