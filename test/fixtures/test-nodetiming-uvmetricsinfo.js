// Enforcing strict checks on the order or number of events across different
// platforms can be tricky and unreliable due to various factors.
// As a result, this test relies on the `uv_metrics_info` call instead.
const { performance } = require('node:perf_hooks');
const assert = require('node:assert');
const fs = require('node:fs');
const { nodeTiming } = performance;

function safeMetricsInfo(cb) {
  setImmediate(() => {
    const info = nodeTiming.uvMetricsInfo;
    cb(info);
  });
}

const kZeroBigInt = { loopCount: 0n, events: 0n, eventsWaiting: 0n };

{
  const info = nodeTiming.uvMetricsInfo;
  assert.strictEqual(info.loopCount, 0);
  assert.strictEqual(info.events, 0);
  // This is the only part of the test that we test events waiting
  // Adding checks for this property will make the test flaky
  // as it can be highly influenced by race conditions.
  assert.strictEqual(info.eventsWaiting, 0);
  assert.deepStrictEqual(nodeTiming.uvMetricsInfoBigInt, kZeroBigInt);
}

{
  // The synchronous call should obviously not affect the uv metrics
  const fd = fs.openSync(__filename, 'r');
  fs.readFileSync(fd);
  const info = nodeTiming.uvMetricsInfo;
  assert.strictEqual(info.loopCount, 0);
  assert.strictEqual(info.events, 0);
  assert.strictEqual(info.eventsWaiting, 0);
  assert.deepStrictEqual(nodeTiming.uvMetricsInfoBigInt, kZeroBigInt);
}

{
  function openFile(info) {
    assert.strictEqual(info.loopCount, 1);
    const infoBigInt = nodeTiming.uvMetricsInfoBigInt;
    assert.strictEqual(infoBigInt.loopCount, 1n);

    fs.open(__filename, 'r', (err) => {
      assert.ifError(err);
    });

    const saved = { ...info };
    const savedBigInt = { ...infoBigInt };
    safeMetricsInfo((nextInfo) => {
      assert.notStrictEqual(nextInfo, info);
      assert.ok(nextInfo.loopCount > saved.loopCount);
      const nextInfoBigInt = nodeTiming.uvMetricsInfoBigInt;
      assert.notStrictEqual(nextInfoBigInt, infoBigInt);
      assert.ok(nextInfoBigInt.loopCount > savedBigInt.loopCount);
      // Updating the shared buffers must not change earlier results.
      assert.deepStrictEqual(info, saved);
      assert.deepStrictEqual(infoBigInt, savedBigInt);
    });
  }

  safeMetricsInfo(openFile);
}

{
  // Both representations are filled by the same native call, and libuv only
  // updates the metrics while the event loop is running, so back-to-back
  // synchronous reads must agree.
  safeMetricsInfo(() => {
    const info = nodeTiming.uvMetricsInfo;
    const infoBigInt = nodeTiming.uvMetricsInfoBigInt;
    for (const key of ['loopCount', 'events', 'eventsWaiting']) {
      assert.strictEqual(typeof info[key], 'number');
      assert.strictEqual(typeof infoBigInt[key], 'bigint');
      assert.strictEqual(BigInt(info[key]), infoBigInt[key]);
    }
  });
}
