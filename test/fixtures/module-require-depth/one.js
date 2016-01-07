// Flags: --expose_internals
'use strict';
const assert = require('assert');
const internalModule = require('internal/module');

exports.requireDepth = internalModule.requireDepth;
assert.strictEqual(internalModule.requireDepth, 1);
assert.deepStrictEqual(require('./two'), { requireDepth: 2 });
assert.strictEqual(internalModule.requireDepth, 1);
