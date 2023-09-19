'use strict';

require('../common');

// Test that a nonexistent "main" entry in a package.json that also omits an
// "exports" entry will be ignored if it can be found in node_modules instead
// rather than throwing.
//
// Throwing is perhaps "correct" behavior, but it will break bluebird tests
// as of this writing.

const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

const testFile = fixtures.path('package-main-enoent', 'test.js');

const { error, status, stderr } = spawnSync(process.execPath, [testFile]);

assert.ifError(error);
assert.strictEqual(status, 0, stderr);
