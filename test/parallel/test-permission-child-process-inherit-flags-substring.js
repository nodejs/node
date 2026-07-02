// Flags: --permission --allow-child-process --allow-fs-read=* --allow-worker
// Tests that NODE_OPTIONS values containing '--permission' as a substring
// in unrelated option values (e.g., --title=--permission) do NOT suppress
// Permission Model flag propagation to child processes.
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}
if (process.config.variables.node_without_node_options) {
  common.skip('missing NODE_OPTIONS support');
}

const assert = require('assert');
const childProcess = require('child_process');

// Verify that the parent has Permission Model enabled
assert.ok(process.permission.has('child'));
assert.strictEqual(process.env.NODE_OPTIONS, undefined);

// Test cases: NODE_OPTIONS values that contain '--permission' as a substring
// but are NOT actual permission flags. These should NOT suppress propagation.
const testCases = [
  { name: 'title', nodeOptions: '--title=--permission' },
  { name: 'conditions', nodeOptions: '--conditions=--permission' },
  { name: 'trace-event-categories', nodeOptions: '--trace-event-categories=--permission' },
  { name: 'title-audit', nodeOptions: '--title=--permission-audit' },
];

for (const { name, nodeOptions } of testCases) {
  // Spawn a child with the problematic NODE_OPTIONS value.
  // Without the fix, the substring check causes propagation to be skipped,
  // and the child will not have process.permission.
  const { status, stdout } = childProcess.spawnSync(
    process.execPath,
    [
      '-e',
      `
      console.log(typeof process.permission);
      console.log(process.permission && process.permission.has("child"));
      console.log(process.env.NODE_OPTIONS);
      `,
    ],
    {
      env: {
        ...process.env,
        'NODE_OPTIONS': nodeOptions,
      }
    }
  );

  assert.strictEqual(status, 0, `child process for ${name} exited with status ${status}`);

  const [permType, hasChild] = stdout.toString().split('\n');

  // Verify the child has Permission Model enabled (the bug caused it to be absent)
  assert.strictEqual(permType, 'object', `child ${name} should have process.permission object`);

  // Verify the child inherited child permission
  assert.strictEqual(hasChild, 'true', `child ${name} should have child permission`);
}

// Also verify that a child with a real --permission flag in NODE_OPTIONS
// still gets its own flags honored (regression test for existing behavior).
{
  const { status, stdout } = childProcess.spawnSync(
    process.execPath,
    [
      '-e',
      `
      console.log(process.permission.has("fs.write"));
      console.log(process.permission.has("fs.read"));
      console.log(process.permission.has("child"));
      `,
    ],
    {
      env: {
        ...process.env,
        'NODE_OPTIONS': '--permission --allow-fs-write=*',
      }
    }
  );

  assert.strictEqual(status, 0);
  const [fsWrite, fsRead, child] = stdout.toString().split('\n');
  assert.strictEqual(fsWrite, 'true');
  assert.strictEqual(fsRead, 'false');
  assert.strictEqual(child, 'false');
}

{
  assert.strictEqual(process.env.NODE_OPTIONS, undefined);
}
