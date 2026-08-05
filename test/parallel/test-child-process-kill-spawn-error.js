'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

// Killing a child process that never spawned must not signal the process
// group of the caller. The check runs in a detached child so that a
// regression cannot take the test runner down with it.
// Refs: https://github.com/nodejs/node/issues/65052
const script = `
  const { spawn } = require('child_process');
  const child = spawn('foo123');
  child.on('error', () => {});
  if (child.kill() !== false || child.killed !== false) process.exit(1);
`;

const child = spawn(process.execPath, ['-e', script], { detached: true });

child.on('exit', common.mustCall((code, signal) => {
  assert.strictEqual(signal, null);
  assert.strictEqual(code, 0);
}));
