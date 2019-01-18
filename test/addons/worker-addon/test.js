'use strict';
const common = require('../../common');
const assert = require('assert');
const child_process = require('child_process');
const path = require('path');
const { Worker } = require('worker_threads');
const binding = path.resolve(__dirname, `./build/${common.buildType}/binding`);

if (process.argv[2] === 'worker') {
  new Worker(`require(${JSON.stringify(binding)});`, { eval: true });
  return;
} else if (process.argv[2] === 'main-thread') {
  process.env.addExtraItemToEventLoop = 'yes';
  require(binding);
  return;
}

for (const test of ['worker', 'main-thread']) {
  const proc = child_process.spawnSync(process.execPath, [
    __filename,
    test
  ]);
  assert.strictEqual(proc.stderr.toString(), '');
  assert.strictEqual(proc.stdout.toString(), 'ctor cleanup dtor');
  assert.strictEqual(proc.status, 0);
}
