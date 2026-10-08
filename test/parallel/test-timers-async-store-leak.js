// Flags: --expose-internals --expose-gc
'use strict';

const common = require('../common');
const { AsyncLocalStorage } = require('async_hooks');
const assert = require('assert');
const {
  async_context_frame,
} = require('internal/timers');

// Stores live in the async context frame, not directly on the resource. The
// resource holds a reference to the frame via async_context_frame.
function assertStore(resource, als, store) {
  assert.strictEqual(resource[async_context_frame]?.get(als), store);
}

function assertNoStore(resource) {
  assert.strictEqual(resource[async_context_frame], undefined);
}

// Test that setTimeout does not retain a reference to the async store after
// firing. The callback and arguments must stay on the Timeout object so that
// refresh() can reactivate the timer.
{
  const asyncLocalStorage = new AsyncLocalStorage();
  const store = {};
  const arg = {};
  asyncLocalStorage.run(store, common.mustCall(() => {
    const timeout = setTimeout(common.mustCall((received) => {
      assert.strictEqual(received, arg);
      setImmediate(common.mustCall(() => {
        assertNoStore(timeout);
      }));
    }), 1, arg);
    assertStore(timeout, asyncLocalStorage, store);
  }));
}

// Test that clearTimeout cleans up the store, callback, and arguments before
// firing.
{
  const asyncLocalStorage = new AsyncLocalStorage();
  const store = {};
  const arg = {};
  asyncLocalStorage.run(store, common.mustCall(() => {
    const timeout = setTimeout(common.mustNotCall(), common.platformTimeout(1000), arg);
    assertStore(timeout, asyncLocalStorage, store);
    clearTimeout(timeout);
    assertNoStore(timeout);
    assert.strictEqual(timeout._onTimeout, undefined);
    assert.strictEqual(timeout._timerArgs, undefined);
  }));
}

// Test that setInterval does not retain a reference to the async store,
// callback, or arguments after it is cleared.
{
  const asyncLocalStorage = new AsyncLocalStorage();
  const store = {};
  const arg = {};
  asyncLocalStorage.run(store, common.mustCall(() => {
    const interval = setInterval(common.mustCall((received) => {
      assert.strictEqual(received, arg);
      assert.strictEqual(asyncLocalStorage.getStore(), store);
      clearInterval(interval);
      assert.strictEqual(asyncLocalStorage.getStore(), store);
      setImmediate(common.mustCall(() => {
        assertNoStore(interval);
        assert.strictEqual(interval._onTimeout, undefined);
        assert.strictEqual(interval._timerArgs, undefined);
      }));
    }), 1, arg);
    assertStore(interval, asyncLocalStorage, store);
  }));
}

// Test that setImmediate does not retain a reference to the async store,
// callback, or arguments after firing.
{
  const asyncLocalStorage = new AsyncLocalStorage();
  const store = {};
  const arg = {};
  asyncLocalStorage.run(store, common.mustCall(() => {
    const immediate = setImmediate(common.mustCall((received) => {
      assert.strictEqual(received, arg);
      setImmediate(common.mustCall(() => {
        assertNoStore(immediate);
        assert.strictEqual(immediate._onImmediate, undefined);
        assert.strictEqual(immediate._argv, undefined);
      }));
    }), arg);
    assertStore(immediate, asyncLocalStorage, store);
  }));
}

// Test that clearImmediate cleans up the store, callback, and arguments before
// firing.
{
  const asyncLocalStorage = new AsyncLocalStorage();
  const store = {};
  const arg = {};
  asyncLocalStorage.run(store, common.mustCall(() => {
    const immediate = setImmediate(common.mustNotCall(), arg);
    assertStore(immediate, asyncLocalStorage, store);
    clearImmediate(immediate);
    assertNoStore(immediate);
    assert.strictEqual(immediate._onImmediate, undefined);
    assert.strictEqual(immediate._argv, undefined);
  }));
}

// Test that the async store reference is cleaned up and can be GC'd.
{
  const asyncLocalStorage = new AsyncLocalStorage();
  let timeout;
  const weakStore = new WeakRef({});
  asyncLocalStorage.run(weakStore.deref(), common.mustCall(() => {
    timeout = setTimeout(common.mustNotCall(), common.platformTimeout(1000));
    assert.strictEqual(weakStore.deref() !== undefined, true);
    clearTimeout(timeout);
  }));
  setImmediate(common.mustCall(() => {
    global.gc();
    assert.strictEqual(weakStore.deref(), undefined);
  }));
}
