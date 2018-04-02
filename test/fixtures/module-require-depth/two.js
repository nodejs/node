// Flags: --expose_internals
'use strict';
const assert = require('assert');
const {
  requireDepth
} = require('internal/modules/cjs/helpers');

exports.requireDepth = requireDepth;
assert.strictEqual(requireDepth, 2);
assert.deepStrictEqual(require('./one'), { requireDepth: 1 });
assert.strictEqual(requireDepth, 2);
