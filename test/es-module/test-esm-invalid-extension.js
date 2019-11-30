'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixture = fixtures.path('/es-modules/import-invalid-ext.mjs');
const child = spawnSync(process.execPath, [fixture]);
const errMsg = 'TypeError [ERR_UNKNOWN_FILE_EXTENSION]: Unknown file extension';
const importMsg = `imported from ${fixture}`;

assert.strictEqual(child.status, 1);
assert.strictEqual(child.signal, null);
assert.strictEqual(child.stdout.toString().trim(), '');
assert(child.stderr.toString().includes(errMsg));
assert(child.stderr.toString().includes(importMsg));
