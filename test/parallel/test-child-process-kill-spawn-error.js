'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { spawn } = require('child_process');

// Killing a child process that never spawned must not signal the process
// group of the caller. The check runs in a detached child so that a
// regression cannot take the test runner down with it.
const childPath = fixtures.path('child-process-kill-spawn-error.js');
const child = spawn(process.execPath, [childPath], { detached: true });

child.on('exit', common.mustCall((code, signal) => {
  assert.strictEqual(signal, null);
  assert.strictEqual(code, 0);
}));
