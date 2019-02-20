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
  case 'worker-twice':
  case 'worker':
    const worker = new Worker(`require(${JSON.stringify(binding)});`, {
      eval: true
    });
    if (process.argv[2] === 'worker-twice') {
      worker.on('exit', common.mustCall(() => {
        new Worker(`require(${JSON.stringify(binding)});`, {
          eval: true
        });
      }));
    }
    return;
  case 'main-thread':
    process.env.addExtraItemToEventLoop = 'yes';
    require(binding);
    return;
}

for (const { test, expected } of [
  { test: 'worker', expected: 'ctor cleanup dtor ' },
  { test: 'main-thread', expected: 'ctor cleanup dtor ' },
  // We always only have 1 instance of the shared object in memory, so
  // 1 ctor and 1 dtor call. If we attach the module to 2 Environments,
  // we expect 2 cleanup calls, otherwise one.
  { test: 'both', expected: 'ctor cleanup cleanup dtor ' },
  // In this case, we load and unload an addon, then load and unload again.
  { test: 'worker-twice', expected: 'ctor cleanup dtor ctor cleanup dtor ' },
]) {
  console.log('spawning test', test);
  const proc = child_process.spawnSync(process.execPath, [
    __filename,
    test
  ]);
  process.stderr.write(proc.stderr.toString());
  assert.strictEqual(proc.stderr.toString(), '');
  assert.strictEqual(
    proc.stdout.toString(),
    expected);
  assert.strictEqual(proc.status, 0);
}
