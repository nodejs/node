'use strict';

const { mustCall } = require('../common');

const TIMEOUT = 10;
const SPIN_DUR = 50;

const assert = require('assert');
const { performance } = require('perf_hooks');
const { Worker, parentPort } = require('worker_threads');

const { nodeTiming, eventLoopUtilization } = performance;
const elu = eventLoopUtilization();

// Take into account whether this test was started as a Worker.
if (nodeTiming.loopStart === -1) {
  assert.strictEqual(nodeTiming.idleTime, 0);
  assert.deepStrictEqual(elu, { idle: 0, active: 0, utilization: 0 });
  assert.deepStrictEqual(eventLoopUtilization(elu),
                         { idle: 0, active: 0, utilization: 0 });
  assert.deepStrictEqual(eventLoopUtilization(elu, eventLoopUtilization()),
                         { idle: 0, active: 0, utilization: 0 });
}

const nodeTimingProps = ['name', 'entryType', 'startTime', 'duration',
                         'nodeStart', 'v8Start', 'environment', 'loopStart',
                         'loopExit', 'bootstrapComplete', 'idleTime'];
for (const p of nodeTimingProps)
  assert.ok(typeof JSON.parse(JSON.stringify(nodeTiming))[p] ===
    typeof nodeTiming[p]);

setTimeout(mustCall(function r() {
  const elu1 = eventLoopUtilization();

  // Force idle time to accumulate before allowing test to continue.
  if (elu1.idle <= 0)
    return setTimeout(mustCall(r), 5);

  const t = Date.now();
  while (Date.now() - t < SPIN_DUR);

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

  setTimeout(mustCall(runIdleTimeTest), TIMEOUT);
}), 5);

function runIdleTimeTest() {
  const idleTime = nodeTiming.idleTime;
  const elu1 = eventLoopUtilization();
  const sum = elu1.idle + elu1.active;

  assert.ok(sum >= elu1.idle && sum >= elu1.active,
            `idle: ${elu1.idle}  active: ${elu1.active}  sum: ${sum}`);
  assert.strictEqual(elu1.idle, idleTime);
  assert.strictEqual(elu1.utilization, elu1.active / sum);

  setTimeout(mustCall(runCalcTest), TIMEOUT, elu1);
}

function runCalcTest(elu1) {
  const now = performance.now();
  const elu2 = eventLoopUtilization();
  const elu3 = eventLoopUtilization(elu2, elu1);
  const active_delta = elu2.active - elu1.active;
  const idle_delta = elu2.idle - elu1.idle;

  assert.ok(elu2.idle >= 0, `${elu2.idle} < 0`);
  assert.ok(elu2.active >= 0, `${elu2.active} < 0`);
  assert.ok(elu3.idle >= 0, `${elu3.idle} < 0`);
  assert.ok(elu3.active >= 0, `${elu3.active} < 0`);
  assert.ok(elu2.idle + elu2.active > elu1.idle + elu1.active,
            `${elu2.idle + elu2.active} <= ${elu1.idle + elu1.active}`);
  assert.ok(elu2.idle + elu2.active >= now - nodeTiming.loopStart,
            `${elu2.idle + elu2.active} < ${now - nodeTiming.loopStart}`);
  assert.strictEqual(elu3.active, elu2.active - elu1.active);
  assert.strictEqual(elu3.idle, elu2.idle - elu1.idle);
  assert.strictEqual(elu3.utilization,
                     active_delta / (idle_delta + active_delta));

  setImmediate(mustCall(runWorkerTest));
}

function runWorkerTest() {
  // Use argv to detect whether we're running as a Worker called by this test
  // vs. this test also being called as a Worker.
  if (process.argv[2] === 'iamalive') {
    parentPort.postMessage(JSON.stringify(eventLoopUtilization()));
    return;
  }

  const elu1 = eventLoopUtilization();
  const worker = new Worker(__filename, { argv: [ 'iamalive' ] });

  worker.on('message', mustCall((msg) => {
    const elu2 = eventLoopUtilization(elu1);
    const data = JSON.parse(msg);

    assert.ok(elu2.active + elu2.idle > data.active + data.idle,
              `${elu2.active + elu2.idle} <= ${data.active + data.idle}`);
  }));
}
