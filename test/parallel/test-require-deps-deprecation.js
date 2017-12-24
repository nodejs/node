'use strict';

const common = require('../common');
const assert = require('assert');
// The v8 modules when imported leak globals. Disable global check.
common.globalCheck = false;

// Newly added deps that do not have a deprecation wrapper around it would
// throw an error, but no warning would be emitted.
const deps = [
  'acorn/dist/acorn',
  'acorn/dist/walk'
];

// Instead of checking require, check that resolve isn't pointing toward a
// built-in module, as user might already have node installed with acorn in
// require.resolve range.
// Ref: https://github.com/nodejs/node/issues/17148
for (const m of deps) {
  let path;
  try {
    path = require.resolve(m);
  } catch (err) {
    assert.ok(err.toString().startsWith('Error: Cannot find module '));
    continue;
  }
  assert.notStrictEqual(path, m);
}
