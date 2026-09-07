import { bench } from 'node:bench';

bench('beta', {
  params: { file: 'b', pid: process.pid },
  samples: 1,
}, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});
