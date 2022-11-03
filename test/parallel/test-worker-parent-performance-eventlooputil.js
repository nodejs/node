'use strict';

const { mustCall } = require('../common');

const TIMEOUT = 10;
const SPIN_DUR = 50;

const assert = require('assert');
const { Worker, parent, workerData } = require('worker_threads');

// Do not use isMainThread directly, otherwise the test would time out in case
// it's started inside of another worker thread.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = '1';
  const i32arr = new Int32Array(new SharedArrayBuffer(4));
  const w = new Worker(__filename, { workerData: i32arr });
  w.on('online', mustCall(() => {
    Atomics.wait(i32arr, 0, 0);

    const t = Date.now();
    while (Date.now() - t < SPIN_DUR);

    Atomics.store(i32arr, 0, 0);
    Atomics.notify(i32arr, 0);
    Atomics.wait(i32arr, 0, 0);
  }));
} else {
  setTimeout(() => {
    const { eventLoopUtilization } = parent.performance;
    const i32arr = workerData;
    const elu1 = eventLoopUtilization();

    Atomics.store(i32arr, 0, 1);
    Atomics.notify(i32arr, 0);
    Atomics.wait(i32arr, 0, 1);

    const elu2 = eventLoopUtilization(elu1);
    const elu3 = eventLoopUtilization();
    const elu4 = eventLoopUtilization(elu3, elu1);

    assert.strictEqual(elu2.idle, 0);
    assert.strictEqual(elu4.idle, 0);
    assert.strictEqual(elu2.utilization, 1);
    assert.strictEqual(elu4.utilization, 1);
    assert.strictEqual(elu3.active - elu1.active, elu4.active);
    assert.ok(elu2.active > SPIN_DUR - 10, `${elu2.active} <= ${SPIN_DUR - 10}`);
    assert.ok(elu2.active < elu4.active, `${elu2.active} >= ${elu4.active}`);
    assert.ok(elu3.active > elu2.active, `${elu3.active} <= ${elu2.active}`);
    assert.ok(elu3.active > elu4.active, `${elu3.active} <= ${elu4.active}`);

    Atomics.store(i32arr, 0, 1);
    Atomics.notify(i32arr, 0);
  }, TIMEOUT);
}
