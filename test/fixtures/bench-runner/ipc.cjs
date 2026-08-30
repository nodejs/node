'use strict';

const { bench } = require('node:bench');

process.send?.({ type: 'user-message' });

bench('user IPC', { samples: 1 }, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});
