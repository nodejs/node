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

const assert = require('assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const pathA = tmpdir.resolve('a'); // file
const pathB = tmpdir.resolve('b'); // directory
const pathC = tmpdir.resolve('c'); // c -> b
const pathD = tmpdir.resolve('d'); // d -> b

writeFileSync(pathA, 'file a');
mkdirSync(pathB);
symlinkSync(pathB, pathC, 'dir');
symlinkSync(pathB, pathD, 'dir');

cp(pathA, pathD, { dereference: false }, common.mustSucceed(() => {
  // The path d is now a file, not a symlink
  assert.strictEqual(readFileSync(pathA, 'utf-8'), readFileSync(pathD, 'utf-8'));
  assert.ok(statSync(pathA).isFile());
  assert.ok(statSync(pathD).isFile());
}));

cpSync(pathA, pathC, { dereference: false });

assert.strictEqual(readFileSync(pathA, 'utf-8'), readFileSync(pathC, 'utf-8'));
assert.ok(statSync(pathA).isFile());
assert.ok(statSync(pathC).isFile());
