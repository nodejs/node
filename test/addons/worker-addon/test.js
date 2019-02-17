'use strict';
const common = require('../../common');
const assert = require('assert');
const child_process = require('child_process');
const path = require('path');
const { Worker } = require('worker_threads');
const binding = path.resolve(__dirname, `./build/${common.buildType}/binding`);

switch (process.argv[2]) {
  case 'both':
    require(binding);
    // fallthrough
  case 'worker':
    new Worker(`require(${JSON.stringify(binding)});`, { eval: true });
    return;
  case 'main-thread':
    process.env.addExtraItemToEventLoop = 'yes';
    require(binding);
    return;
}

for (const test of ['worker', 'main-thread', 'both']) {
  const proc = child_process.spawnSync(process.execPath, [
    __filename,
    test
  ]);
  assert.strictEqual(proc.stderr.toString(), '');
  // We always only have 1 instance of the shared object in memory, so
  // 1 ctor and 1 dtor call. If we attach the module to 2 Environments,
  // we expect 2 cleanup calls, otherwise one.
  assert.strictEqual(
    proc.stdout.toString(),
    test === 'both' ? 'ctor cleanup cleanup dtor' : 'ctor cleanup dtor');
  assert.strictEqual(proc.status, 0);
}
