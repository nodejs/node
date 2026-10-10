import { once } from 'node:events';
import { createRequire } from 'node:module';
import { Worker as ThreadWorker } from 'node:worker_threads';
import staticImport from 'pkg';
import custom from 'pkg/custom';
import workerOnly from 'pkg/worker-only';

const require = createRequire(import.meta.url);

const { default: dynamicImport } = await import('pkg');

const thread = new ThreadWorker(new URL('./thread.mjs', import.meta.url));
const [threadResult] = await once(thread, 'message');

const nested = new Worker(new URL('./nested-worker.mjs', import.meta.url), { type: 'module' });
const nestedResult = await new Promise((resolve, reject) => {
  nested.onmessage = ({ data }) => resolve(data);
  nested.onerror = reject;
});
nested.terminate();

postMessage({
  staticImport,
  dynamicImport,
  custom,
  workerOnly,
  resolve: import.meta.resolve('pkg'),
  require: require('pkg'),
  thread: threadResult,
  nested: nestedResult,
});
