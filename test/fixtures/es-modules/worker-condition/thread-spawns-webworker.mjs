import { parentPort } from 'node:worker_threads';
import value from 'pkg';

const worker = new Worker(new URL('./nested-worker.mjs', import.meta.url), { type: 'module' });
worker.onmessage = ({ data }) => {
  worker.terminate();
  parentPort.postMessage({ thread: value, webWorker: data });
};
