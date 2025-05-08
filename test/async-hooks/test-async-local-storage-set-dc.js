'use strict';

if (process.execArgv.includes('--no-async-context-frame')) {
  // Skipping test as it is only relevant when async_context_frame is not disabled.
  process.exit(0);
}

const assert = require('assert');
const { subscribe } = require('diagnostics_channel');
const { AsyncLocalStorage, AsyncResource } = require('async_hooks');

let frameSetCounter = 0;
subscribe('async_context_frame.set', () => {
  frameSetCounter++;
});

const asyncLocalStorage = new AsyncLocalStorage();
assert.equal(frameSetCounter, 0);
let lastObservedSetCounter = 0;

function assertFrameWasSet() {
  assert(frameSetCounter > lastObservedSetCounter);
  lastObservedSetCounter = frameSetCounter;
}

setImmediate(() => {
  // Entering an immediate callback sets the frame.
  assertFrameWasSet();
  const store = { foo: 'bar' };
  asyncLocalStorage.enterWith(store);
  // enterWith sets the frame.
  assertFrameWasSet();
  assert.strictEqual(asyncLocalStorage.getStore(), store);

  setTimeout(() => {
    // Entering a timeout callback sets the frame.
    assertFrameWasSet();
    assert.strictEqual(asyncLocalStorage.getStore(), store);
    const res = new AsyncResource('test');
    const store2 = { foo: 'bar2' };
    asyncLocalStorage.enterWith(store2);
    // enterWith sets the frame.
    assertFrameWasSet();
    res.runInAsyncScope(() => {
      // runInAsyncScope sets the frame on entry.
      assertFrameWasSet();
      // AsyncResource was constructed before store2 assignment, so it should
      // keep reflecting the old store.
      assert.strictEqual(asyncLocalStorage.getStore(), store);
    });
    // runInAsyncScope sets the frame on exit.
    assertFrameWasSet();
  }, 10);
});
