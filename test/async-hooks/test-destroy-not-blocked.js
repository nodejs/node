'use strict';
// Flags: --expose_gc

const common = require('../common');
const assert = require('assert');
const tick = require('../common/tick');

const { createHook, AsyncResource } = require('async_hooks');

// Test priority of destroy hook relative to nextTick,... and
// verify a microtask is scheduled in case a lot items are queued

const resType = 'MyResource';
let activeId = -1;
createHook({
  init(id, type) {
    if (type === resType) {
      assert.strictEqual(activeId, -1);
      activeId = id;
    }
  },
  destroy(id) {
    if (activeId === id) {
      activeId = -1;
    }
  },
}).enable();

function testNextTick() {
  assert.strictEqual(activeId, -1);
  const res = new AsyncResource(resType);
  assert.strictEqual(activeId, res.asyncId());
  res.emitDestroy();
  // nextTick has higher prio than emit destroy
  process.nextTick(common.mustCall(() =>
    assert.strictEqual(activeId, res.asyncId())),
  );
}

function testQueueMicrotask() {
  assert.strictEqual(activeId, -1);
  const res = new AsyncResource(resType);
  assert.strictEqual(activeId, res.asyncId());
  res.emitDestroy();
  // queueMicrotask has higher prio than emit destroy
  queueMicrotask(common.mustCall(() =>
    assert.strictEqual(activeId, res.asyncId())),
  );
}

function testImmediate() {
  assert.strictEqual(activeId, -1);
  const res = new AsyncResource(resType);
  assert.strictEqual(activeId, res.asyncId());
  res.emitDestroy();
  setImmediate(common.mustCall(() =>
    assert.strictEqual(activeId, -1)),
  );
}

function testPromise() {
  assert.strictEqual(activeId, -1);
  const res = new AsyncResource(resType);
  assert.strictEqual(activeId, res.asyncId());
  res.emitDestroy();
  // Promise has higher prio than emit destroy
  Promise.resolve().then(common.mustCall(() =>
    assert.strictEqual(activeId, res.asyncId())),
  );
}

async function testAwait() {
  assert.strictEqual(activeId, -1);
  const res = new AsyncResource(resType);
  assert.strictEqual(activeId, res.asyncId());
  res.emitDestroy();

  for (let i = 0; i < 5000; i++) {
    await Promise.resolve();
  }
  global.gc();
  await Promise.resolve();
  // Limit to trigger a microtask not yet reached
  assert.strictEqual(activeId, res.asyncId());
  for (let i = 0; i < 5000; i++) {
    await Promise.resolve();
  }
  global.gc();
  await Promise.resolve();
  assert.strictEqual(activeId, -1);
}

testNextTick();
tick(2, testQueueMicrotask);
tick(4, testImmediate);
tick(6, testPromise);
tick(8, () => testAwait().then(common.mustCall()));
