'use strict';

const { PerformanceObserver } = require('node:perf_hooks');
const { setDeserializeMainFunction } = require('node:v8').startupSnapshot;

// Observe 'gc' entries while building the snapshot.
let received = 0;
const observer = new PerformanceObserver((list) => {
  received += list.getEntries().length;
});
observer.observe({ type: 'gc' });

// Performance entries are dispatched asynchronously, so trigger GCs until the
// entries arrive.
function waitForEntries(getCount, callback, attempts = 10) {
  globalThis.gc();
  setImmediate(() => {
    if (getCount() > 0) {
      callback();
    } else if (attempts > 1) {
      waitForEntries(getCount, callback, attempts - 1);
    } else {
      throw new Error('No gc entries were received after deserialization');
    }
  });
}

setDeserializeMainFunction(() => {
  // The GC callbacks registered while building the snapshot do not survive
  // it, so they must be registered again after deserialization.
  if (process.env.TEST_NEW_OBSERVER) {
    // Observing 'gc' again after deserialization.
    let newReceived = 0;
    const newObserver = new PerformanceObserver((list) => {
      newReceived += list.getEntries().length;
    });
    newObserver.observe({ type: 'gc' });

    waitForEntries(() => newReceived, () => {
      // Disconnecting must only remove GC callbacks that are registered.
      newObserver.disconnect();
      observer.disconnect();
      console.log('ok');
    });
  } else {
    // The observer that was active while building the snapshot.
    waitForEntries(() => received, () => {
      observer.disconnect();
      console.log('ok');
    });
  }
});
