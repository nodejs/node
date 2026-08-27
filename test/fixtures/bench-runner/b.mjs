import { bench } from 'node:bench';

bench('beta', {
  params: { file: 'b', pid: process.pid },
  samples: 1,
}, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});
