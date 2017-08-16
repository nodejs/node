// Flags: --expose_internals
'use strict';
const assert = require('assert');
const internalModule = require('internal/module');

exports.requireDepth = internalModule.requireDepth;
assert.strictEqual(internalModule.requireDepth, 2);
assert.deepStrictEqual(require('./one'), { requireDepth: 1 });
assert.strictEqual(internalModule.requireDepth, 2);
