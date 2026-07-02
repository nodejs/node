'use strict';
require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');

// This test ensures that using diagnostics_channel does not cause a segfault
// when explicit resource management is disabled in V8.
// Refs: https://github.com/nodejs/node/issues/64230

const child = spawnSync(process.execPath, [
  '--no-js-explicit-resource-management',
  '--eval',
  'require("diagnostics_channel").tracingChannel("foo").traceSync(() => {})'
]);

// If it segfaults, signal will be something like 'SIGSEGV'.
// We assert that the process exited cleanly (status 0).
assert.strictEqual(child.signal, null);
assert.strictEqual(child.status, 0);
assert.strictEqual(child.stderr.toString(), '');