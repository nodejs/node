'use strict';

// An exception thrown from the call_js callback of a thread-safe function is
// reported through 'uncaughtException'. The listener runs while Node-API is
// still handling that exception, so calling back into the module from it,
// directly or through a finalizer run by the garbage collector, must not see
// the exception as pending and report it a second time.
// Refs: https://github.com/nodejs/node/issues/67421

const common = require('../../common');
const assert = require('assert');

const [, , addonName, scenario] = process.argv;

if (addonName === undefined) {
  const { spawnSyncAndAssert } = require('../../common/child_process');
  for (const [name, scenarios] of [
    // Finalizers run inside the garbage collector in experimental modules.
    ['test_uncaught_exception_reentry', ['gc', 'call']],
    ['test_uncaught_exception_reentry_v10', ['call']],
  ]) {
    for (const scenario of scenarios) {
      spawnSyncAndAssert(
        process.execPath,
        ['--expose-gc', __filename, name, scenario],
        { stdout: 'listener finished\n', stderr: '' },
      );
    }
  }
  return;
}

const binding = require(`./build/${common.buildType}/${addonName}`);

// Objects with a native finalizer, kept alive until the listener runs so that
// they are collected by the gc() call in it.
let wrapped = [];

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, 'call_js error');
  if (scenario === 'gc') {
    wrapped = null;
    globalThis.gc();
  } else {
    assert.strictEqual(binding.CallFunction(() => 42), 42);
  }
  console.log('listener finished');
}));

if (scenario === 'gc') {
  for (let i = 0; i < 100; i++) {
    const object = {};
    binding.Wrap(object);
    wrapped.push(object);
  }
}

binding.ThrowFromThreadSafeFunction();
