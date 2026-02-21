'use strict';

// Refs: https://github.com/nodejs/node/issues/58939
//
// In this test, both the cp and cpSync functions are attempting to copy
// a file over a symlinked directory.

require('../common');

const {
  cpSync,
  symlinkSync,
  writeFileSync,
  readFileSync,
  statSync,
  lstatSync,
} = require('fs');

const {
  join,
} = require('path');

const assert = require('assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const pathA = join(tmpdir.path, 'a');
const pathC = join(tmpdir.path, 'c');
const pathD = join(tmpdir.path, 'd');

writeFileSync(pathA, 'file a');
assert.ok(statSync(pathA).isFile());
symlinkSync(pathA, pathC); // c -> a
assert.ok(lstatSync(pathC).isSymbolicLink());
assert.ok(statSync(pathC).isFile());

cpSync(pathC, pathD, { dereference: true });
// d/c -> a
// d/c
assert.ok(statSync(pathD).isFile());
assert.strictEqual(readFileSync(pathD, 'utf8'), 'file a');
