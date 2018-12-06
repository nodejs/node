// Flags: --experimental-worker
'use strict';
const common = require('../../common');
const assert = require('assert');
const child_process = require('child_process');
const path = require('path');
const { Worker } = require('worker_threads');
const binding = path.resolve(__dirname, `./build/${common.buildType}/binding`);

if (process.argv[2] === 'child') {
  new Worker(`require(${JSON.stringify(binding)});`, { eval: true });
} else {
  const proc = child_process.spawnSync(process.execPath, [
    '--experimental-worker',
    __filename,
    'child'
  ]);
  assert.strictEqual(proc.stderr.toString(), '');
  assert.strictEqual(proc.stdout.toString(), 'ctor cleanup dtor');
  assert.strictEqual(proc.status, 0);
}
