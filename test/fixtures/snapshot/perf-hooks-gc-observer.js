'use strict';

const { PerformanceObserver } = require('node:perf_hooks');
const { setDeserializeMainFunction } = require('node:v8').startupSnapshot;

// Observe 'gc' entries while building the snapshot.
const observer = new PerformanceObserver(() => {});
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
  // it. Observing 'gc' after deserialization must register them again.
  let received = 0;
  const newObserver = new PerformanceObserver((list) => {
    received += list.getEntries().length;
  });
  newObserver.observe({ type: 'gc' });

  waitForEntries(() => received, () => {
    // Disconnecting must only remove GC callbacks that are registered.
    newObserver.disconnect();
    observer.disconnect();
    console.log('ok');
  });
});
