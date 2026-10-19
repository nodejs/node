import { parentPort } from 'node:worker_threads';
import { createRequire } from 'node:module';
import value from 'pkg';
import custom from 'pkg/custom';

const require = createRequire(import.meta.url);

parentPort.postMessage({
  import: value,
  custom,
  require: require('pkg'),
  resolve: import.meta.resolve('pkg'),
});
