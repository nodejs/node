// Flags: --expose_internals
'use strict';
const common = require('../common');
const assert = require('assert');
const internalModule = require('internal/module');

// Module one loads two too so the expected depth for two is, well, two.
assert.strictEqual(internalModule.requireDepth, 0);
const one = require(`${common.fixturesDir}/module-require-depth/one`);
const two = require(`${common.fixturesDir}/module-require-depth/two`);
assert.deepStrictEqual(one, { requireDepth: 1 });
assert.deepStrictEqual(two, { requireDepth: 2 });
assert.strictEqual(internalModule.requireDepth, 0);
