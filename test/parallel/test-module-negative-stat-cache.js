'use strict';
require('../common');

// This tests that the CommonJS loader's per-require-tree stat cache also caches
// negative (not-found) results, so a path that is missing when first probed is
// not re-stat-ed for the rest of the require tree.

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// A module path that does not exist yet.
const generated = tmpdir.resolve('generated.js');

// First probe: the file does not exist -> negative stat, cached.
assert.throws(
  () => require(generated),
  { code: 'MODULE_NOT_FOUND' },
  'expected the module to be missing before it is created',
);

// Create the file mid-traversal, in the same require tree.
fs.writeFileSync(generated, 'module.exports = 1;');

// Second probe, still in the same tree: the negative result is cached, so the
// loader must serve the cached miss instead of re-stat-ing and observing the
// freshly-created file.
assert.throws(
  () => require(generated),
  { code: 'MODULE_NOT_FOUND' },
  'a negative stat result must be cached for the rest of the require tree',
);
