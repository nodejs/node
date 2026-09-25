'use strict';

// Keep the event loop alive.
setTimeout(() => {}, 1e6);

process.on('uncaughtExceptionMonitor', (err) => {
  console.log(`Monitored: ${err.message}`);
});

throw new Error('Shall exit');
