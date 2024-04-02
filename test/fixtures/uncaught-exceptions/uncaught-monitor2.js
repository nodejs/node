'use strict';

// Keep the event loop alive.
setTimeout(() => {}, 1e6);

process.on('uncaughtExceptionMonitor', (err) => {
  console.log(`Monitored: ${err.message}, will throw now`);
  missingFunction();
});

throw new Error('Shall exit');
