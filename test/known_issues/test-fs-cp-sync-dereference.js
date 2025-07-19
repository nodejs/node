'use strict';

// Refs: https://github.com/nodejs/node/issues/58939
//
// The cpSync function is not correctly handling the `dereference` option.
// In this test, both the cp and cpSync functions are attempting to copy
// a file over a symlinked directory. In the cp case it works fine. In the
// cpSync case it fails with an error.

const common = require('../common');

const {
  cp,
  cpSync,
  mkdirSync,
  symlinkSync,
  writeFileSync,
} = require('fs');

const {
  join,
} = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const pathA = join(tmpdir.path, 'a');
const pathB = join(tmpdir.path, 'b');
const pathC = join(tmpdir.path, 'c');
const pathD = join(tmpdir.path, 'd');

writeFileSync(pathA, 'file a');
mkdirSync(pathB);
symlinkSync(pathB, pathC, 'dir');
symlinkSync(pathB, pathD, 'dir');

cp(pathA, pathD, { dereference: false }, common.mustSucceed());

cpSync(pathA, pathC, { dereference: false });
