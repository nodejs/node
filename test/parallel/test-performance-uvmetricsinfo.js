'use strict';

require('../common');

const fixtures = require('../common/fixtures');

const filepath = fixtures.path('x.txt');

const assert = require('node:assert');
const fs = require('node:fs');
const { performance } = require('node:perf_hooks');

const { uvMetricsInfo } = performance;

function safeMetricsInfo(cb) {
  setImmediate(() => {
    const info = uvMetricsInfo();
    cb(info);
  });
}

{
  const info = uvMetricsInfo();
  assert.strictEqual(info.loopCount, 0);
  assert.strictEqual(info.events, 0);
  // This is the only part of the test that we test events waiting
  // Adding checks for this property will make the test flaky
  // as it can be highly influenced by race conditions.
  assert.strictEqual(info.eventsWaiting, 0);
}

{
  // The synchronous call should obviously not affect the uv metrics
  const fd = fs.openSync(filepath, 'r');
  fs.readFileSync(fd);
  const info = uvMetricsInfo();
  assert.strictEqual(info.loopCount, 0);
  assert.strictEqual(info.events, 0);
  assert.strictEqual(info.eventsWaiting, 0);
}

{
  function openFile(info) {
    assert.strictEqual(info.loopCount, 1);
    // 1. ? event
    assert.strictEqual(info.events, 1);

    fs.open(filepath, 'r', (err) => {
      assert.ifError(err);
      safeMetricsInfo(afterOpenFile);
    });
  }

  function afterOpenFile(info) {
    assert.strictEqual(info.loopCount, 2);
    // 1. ? event
    // 2. uv_fs_open
    assert.strictEqual(info.events, 2);

    fs.readFile(filepath, (err) => {
      assert.ifError(err);
      safeMetricsInfo(afterReadFile);
    });
  }

  function afterReadFile(info) {
    assert.strictEqual(info.loopCount, 6);
    // 1. ? event
    assert.strictEqual(info.events, 6);
    // 1. ?
  }

  safeMetricsInfo(openFile);
}
