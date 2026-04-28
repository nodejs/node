'use strict';
const common = require('../common');
const { isSunOS } = require('../common');
const assert = require('assert');
const {
  Worker,
} = require('worker_threads');

function validate(result) {
  assert.ok(typeof result == 'object' && result !== null);
  assert.ok(result.user >= 0);
  assert.ok(result.system >= 0);
  assert.ok(Number.isFinite(result.user));
  assert.ok(Number.isFinite(result.system));
}

function check(worker) {
  [
    -1,
    1.1,
    NaN,
    undefined,
    {},
    [],
    null,
    function() {},
    Symbol(),
    true,
    Infinity,
    { user: -1, system: 1 },
    { user: 1, system: -1 },
  ].forEach((value) => {
    try {
      worker.cpuUsage(value);
    } catch (e) {
      assert.ok(/ERR_OUT_OF_RANGE|ERR_INVALID_ARG_TYPE/i.test(e.code));
    }
  });
}

const worker = new Worker(`
  const { parentPort } = require('worker_threads');
  parentPort.on('message', () => {});
  `, { eval: true });

// See test-process-threadCpuUsage-main-thread.js
if (isSunOS) {
  assert.throws(
    () => worker.cpuUsage(),
    {
      code: 'ERR_OPERATION_FAILED',
      name: 'Error',
      message: 'Operation failed: worker.cpuUsage() is not available on SunOS'
    }
  );
  worker.terminate();
} else {
  worker.on('online', common.mustCall(async () => {
    check(worker);

    const prev = await worker.cpuUsage();
    validate(prev);

    const curr = await worker.cpuUsage();
    validate(curr);

    assert.ok(curr.user >= prev.user);
    assert.ok(curr.system >= prev.system);

    const delta = await worker.cpuUsage(curr);
    validate(delta);

    worker.terminate();
  }));

  worker.once('exit', common.mustCall(async () => {
    await assert.rejects(worker.cpuUsage(), {
      code: 'ERR_WORKER_NOT_RUNNING'
    });
  }));
}
