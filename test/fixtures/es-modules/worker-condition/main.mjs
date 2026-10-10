import { createRequire } from 'node:module';
import value from 'pkg';
import custom from 'pkg/custom';

const require = createRequire(import.meta.url);

export default {
  import: value,
  custom,
  require: require('pkg'),
  resolve: import.meta.resolve('pkg'),
  workerOnly: await import('pkg/worker-only').then(() => 'resolved', (err) => err.code),
};
