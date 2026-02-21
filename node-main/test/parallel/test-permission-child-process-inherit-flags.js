// Flags: --permission --allow-child-process --allow-fs-read=* --allow-worker
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const childProcess = require('child_process');

{
  assert.ok(process.permission.has('child'));
}

{
  assert.strictEqual(process.env.NODE_OPTIONS, undefined);
}

{
  const { status, stdout } = childProcess.spawnSync(process.execPath,
                                                    [
                                                      '-e',
                                                      `
      console.log(process.permission.has("fs.write"));
      console.log(process.permission.has("fs.read"));
      console.log(process.permission.has("child"));
      console.log(process.permission.has("net"));
      console.log(process.permission.has("worker"));
      `,
                                                    ]
  );
  const [fsWrite, fsRead, child, net, worker] = stdout.toString().split('\n');
  assert.strictEqual(status, 0);
  assert.strictEqual(fsWrite, 'false');
  assert.strictEqual(fsRead, 'true');
  assert.strictEqual(child, 'true');
  assert.strictEqual(net, 'false');
  assert.strictEqual(worker, 'true');
}

// It should not override when --permission is passed
{
  const { status, stdout } = childProcess.spawnSync(
    process.execPath,
    [
      '--permission',
      '--allow-fs-write=*',
      '-e',
      `
      console.log(process.permission.has("fs.write"));
      console.log(process.permission.has("fs.read"));
      console.log(process.permission.has("child"));
      console.log(process.permission.has("net"));
      console.log(process.permission.has("worker"));
      `,
    ]
  );
  const [fsWrite, fsRead, child, net, worker] = stdout.toString().split('\n');
  assert.strictEqual(status, 0);
  assert.strictEqual(fsWrite, 'true');
  assert.strictEqual(fsRead, 'false');
  assert.strictEqual(child, 'false');
  assert.strictEqual(net, 'false');
  assert.strictEqual(worker, 'false');
}

// It should not override when NODE_OPTIONS with --permission is passed
{
  const { status, stdout } = childProcess.spawnSync(
    process.execPath,
    [
      '-e',
      `
      console.log(process.permission.has("fs.write"));
      console.log(process.permission.has("fs.read"));
      console.log(process.permission.has("child"));
      console.log(process.permission.has("net"));
      console.log(process.permission.has("worker"));
      `,
    ],
    {
      env: {
        ...process.env,
        'NODE_OPTIONS': '--permission --allow-fs-write=*',
      }
    }
  );
  const [fsWrite, fsRead, child, net, worker] = stdout.toString().split('\n');
  assert.strictEqual(status, 0);
  assert.strictEqual(fsWrite, 'true');
  assert.strictEqual(fsRead, 'false');
  assert.strictEqual(child, 'false');
  assert.strictEqual(net, 'false');
  assert.strictEqual(worker, 'false');
}

{
  assert.strictEqual(process.env.NODE_OPTIONS, undefined);
}
