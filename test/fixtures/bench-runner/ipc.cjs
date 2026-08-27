'use strict';

const { bench } = require('node:bench');

process.send?.({ type: 'user-message' });

bench('user IPC', { samples: 1 }, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});
