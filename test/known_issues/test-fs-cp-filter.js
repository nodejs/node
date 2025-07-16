'use strict';

// This test will fail because the the implementation does not properly
// handle the case when the `src` or `dest` is a Buffer and the `filter`
// function is utilized when recursively copying directories.
// Refs: https://github.com/nodejs/node/issues/58634
// Refs: https://github.com/nodejs/node/issues/58869

const common = require('../common');

const {
  cpSync,
  mkdirSync,
} = require('fs');

const {
  join,
} = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const pathA = join(tmpdir.path, 'a');
const pathAC = join(pathA, 'c');
const pathB = join(tmpdir.path, 'b');
mkdirSync(pathAC, { recursive: true });

cpSync(Buffer.from(pathA), Buffer.from(pathB), {
  recursive: true,
  // This should be called multiple times, once for each file/directory,
  // but it's only called once in this test because we're expecting this
  // to fail.
  filter: common.mustCall(() => true, 1),
});
