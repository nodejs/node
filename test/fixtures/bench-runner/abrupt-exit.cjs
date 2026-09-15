'use strict';

const mode = process.env.NODE_BENCH_EXIT_MODE;

if (mode === 'code') process.exit(2);
if (mode === 'signal') process.kill(process.pid, 'SIGTERM');

const { bench } = require('node:bench');

if (mode === 'late') {
  process.on('beforeExit', () => { process.exitCode = 2; });
}

bench('abrupt exit', { samples: 1 }, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});
