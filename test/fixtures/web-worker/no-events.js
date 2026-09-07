'use strict';

const dispatched = [];
for (const name of ['languagechange', 'online', 'offline',
                    'rejectionhandled', 'unhandledrejection']) {
  addEventListener(name, () => dispatched.push(name));
}

let processEvents = 0;
process.on('unhandledRejection', () => {
  processEvents++;
  // Give the event target a chance to fire before reporting back.
  setImmediate(() => postMessage({
    dispatched,
    processEvents,
  }));
});

Promise.reject(new Error('reported through process, not the event target'));
