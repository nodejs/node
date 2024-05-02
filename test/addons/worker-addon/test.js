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
  case 'worker': {
    const worker = new Worker(`require(${JSON.stringify(binding)});`, {
      eval: true,
    });
    if (process.argv[2] === 'worker-twice') {
      worker.on('exit', common.mustCall(() => {
        new Worker(`require(${JSON.stringify(binding)});`, {
          eval: true,
        });
      }));
    }
    return;
  }
  case 'main-thread':
    process.env.addExtraItemToEventLoop = 'yes';
    require(binding);
    return;
}

// Use process.report to figure out if we might be running under musl libc.
const glibc = process.report.getReport().header.glibcVersionRuntime;
assert(typeof glibc === 'string' || glibc === undefined, glibc);

const libcMayBeMusl = common.isLinux && glibc === undefined;

for (const { test, expected } of [
  { test: 'worker', expected: [ 'ctor cleanup dtor ' ] },
  { test: 'main-thread', expected: [ 'ctor cleanup dtor ' ] },
  // We always only have 1 instance of the shared object in memory, so
  // 1 ctor and 1 dtor call. If we attach the module to 2 Environments,
  // we expect 2 cleanup calls, otherwise one.
  { test: 'both', expected: [ 'ctor cleanup cleanup dtor ' ] },
  {
    test: 'worker-twice',
    // In this case, we load and unload an addon, then load and unload again.
    // musl doesn't support unloading, so the output may be missing
    // a dtor + ctor pair.
    expected: [
      'ctor cleanup dtor ctor cleanup dtor ',
    ].concat(libcMayBeMusl ? [
      'ctor cleanup cleanup dtor ',
    ] : []),
  },
]) {
  console.log('spawning test', test);
  const proc = child_process.spawnSync(process.execPath, [
    __filename,
    test,
  ]);
  process.stderr.write(proc.stderr.toString());
  assert.strictEqual(proc.stderr.toString(), '');
  assert(expected.includes(proc.stdout.toString()),
         `${proc.stdout.toString()} is not included in ${expected}`);
  assert.strictEqual(proc.status, 0);
}
