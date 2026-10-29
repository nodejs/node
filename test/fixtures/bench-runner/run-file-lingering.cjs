'use strict';

const { bench } = require('node:bench');

setInterval(() => {}, 1_000);
if (process.platform !== 'win32') process.on('SIGTERM', () => {});
process.on('disconnect', () => process.stdout.write('child disconnected\n'));

bench('lingering run file', { samples: 1 }, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});
