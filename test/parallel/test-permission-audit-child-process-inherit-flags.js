// Flags: --permission-audit --allow-child-process --allow-fs-read=* --allow-fs-write=*
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

// Verify that the parent is running in audit mode
assert.strictEqual(typeof process.permission.has, 'function');

{
  assert.strictEqual(process.env.NODE_OPTIONS, undefined);
}

// Child should inherit --permission-audit and the allow-flags via NODE_OPTIONS
{
  const { status, stdout } = childProcess.spawnSync(process.execPath,
                                                    [
                                                      '-e',
                                                      `
      console.log(typeof process.permission);
      console.log(process.permission.has("fs.write"));
      console.log(process.permission.has("fs.read"));
      console.log(process.permission.has("child"));
      `,
                                                    ]
  );
  assert.strictEqual(status, 0);
  const [permType, fsWrite, fsRead, child] = stdout.toString().split('\n');
  assert.strictEqual(permType, 'object');
  assert.strictEqual(fsWrite, 'true');
  assert.strictEqual(fsRead, 'true');
  assert.strictEqual(child, 'true');
}

// Child spawned with explicit --permission should use its own flags, not inherit parent's
{
  const { status, stdout } = childProcess.spawnSync(
    process.execPath,
    [
      '--permission',
      '--allow-fs-write=*',
      '-e',
      `
      console.log(typeof process.permission);
      console.log(process.permission.has("fs.write"));
      console.log(process.permission.has("fs.read"));
      console.log(process.permission.has("child"));
      `,
    ]
  );
  assert.strictEqual(status, 0);
  const [permType, fsWrite, fsRead, child] = stdout.toString().split('\n');
  assert.strictEqual(permType, 'object');
  assert.strictEqual(fsWrite, 'true');
  assert.strictEqual(fsRead, 'false');
  assert.strictEqual(child, 'false');
}

// Child spawned with explicit --permission-audit should use its own flags
{
  const { status, stdout } = childProcess.spawnSync(
    process.execPath,
    [
      '--permission-audit',
      '--allow-fs-write=*',
      '-e',
      `
      console.log(typeof process.permission);
      console.log(process.permission.has("fs.write"));
      console.log(process.permission.has("fs.read"));
      `,
    ]
  );
  assert.strictEqual(status, 0);
  const [permType, fsWrite, fsRead] = stdout.toString().split('\n');
  assert.strictEqual(permType, 'object');
  assert.strictEqual(fsWrite, 'true');
  assert.strictEqual(fsRead, 'false');
}

{
  assert.strictEqual(process.env.NODE_OPTIONS, undefined);
}
