'use strict';

// Tests that code coverage is written when --process-timeout expires while the
// main thread is executing JavaScript.

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const { spawnSyncAndExit } = require('../common/child_process');

tmpdir.refresh();

spawnSyncAndExit(process.execPath, [
  `--process-timeout=${common.platformTimeout(1000)}ms`,
  '-e',
  'function spin() { for (;;); } spin();',
], {
  env: { ...process.env, NODE_V8_COVERAGE: tmpdir.path },
}, {
  status: 124,
  signal: null,
});

const files = fs.readdirSync(tmpdir.path);
assert.strictEqual(files.length, 1);
assert.match(files[0], /^coverage-\d+-\d+-\d+\.json$/);
const coverage = JSON.parse(fs.readFileSync(tmpdir.resolve(files[0]), 'utf8'));
assert.ok(coverage.result.length > 0);
