// Flags: --expose_internals
'use strict';
const assert = require('assert');
const {
  requireDepth
} = require('internal/modules/cjs/helpers');

exports.requireDepth = requireDepth;
assert.strictEqual(requireDepth, 1);
assert.deepStrictEqual(require('./two'), { requireDepth: 2 });
assert.strictEqual(requireDepth, 1);
