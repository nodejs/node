'use strict';
require('../common');

if (process.execArgv.includes('--no-async-context-frame')) {
  // Skipping test as it is only relevant when async_context_frame is not disabled.
  process.exit(0);
}

const assert = require('assert');
const { subscribe } = require('diagnostics_channel');
const { AsyncLocalStorage, AsyncResource } = require('async_hooks');

let frameSetCounter = 0;
let lastPublishedFrame = 0;
subscribe('async_context_frame.set', (frame) => {
  frameSetCounter++;
  lastPublishedFrame = frame;
});

const asyncLocalStorage = new AsyncLocalStorage();
assert.strictEqual(frameSetCounter, 0);
let lastObservedSetCounter = 0;

function assertFrameWasSet(expectedStore) {
  assert(frameSetCounter > lastObservedSetCounter);
  assert.strictEqual(lastPublishedFrame?.get(asyncLocalStorage), expectedStore);
  lastObservedSetCounter = frameSetCounter;
}

setImmediate(() => {
  // Entering an immediate callback sets the frame.
  assertFrameWasSet(undefined);
  const store = { foo: 'bar' };
  asyncLocalStorage.enterWith(store);
  // enterWith sets the frame.
  assertFrameWasSet(store);
  assert.strictEqual(asyncLocalStorage.getStore(), store);

  setTimeout(() => {
    // Entering a timeout callback sets the frame.
    assertFrameWasSet(store);
    assert.strictEqual(asyncLocalStorage.getStore(), store);
    const res = new AsyncResource('test');
    const store2 = { foo: 'bar2' };
    asyncLocalStorage.enterWith(store2);
    // enterWith sets the frame.
    assertFrameWasSet(store2);
    res.runInAsyncScope(() => {
      // runInAsyncScope sets the frame on entry.
      assertFrameWasSet(store);
      // AsyncResource was constructed before store2 assignment, so it should
      // keep reflecting the old store.
      assert.strictEqual(asyncLocalStorage.getStore(), store);
    });
    // runInAsyncScope sets the frame on exit.
    assertFrameWasSet(store2);
  }, 10);
});
