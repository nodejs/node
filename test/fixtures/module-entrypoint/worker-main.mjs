import { entrypoint } from 'node:module';
import { Worker } from 'node:worker_threads';
import { once } from 'node:events';

const fileWorker = new Worker(new URL('./worker.cjs', import.meta.url));
const [fileWorkerEntrypoint] = await once(fileWorker, 'message');

const evalWorker = new Worker(
  'require("node:worker_threads").parentPort.postMessage(' +
  'String(require("node:module").entrypoint));',
  { eval: true },
);
const [evalWorkerEntrypoint] = await once(evalWorker, 'message');

const dataURL = 'data:text/javascript,' + encodeURIComponent(
  'import { entrypoint } from "node:module";' +
  'import { parentPort } from "node:worker_threads";' +
  'parentPort.postMessage(entrypoint);',
);
const dataURLWorker = new Worker(new URL(dataURL));
const [dataURLWorkerEntrypoint] = await once(dataURLWorker, 'message');

console.log(JSON.stringify({
  entrypoint,
  fileWorkerEntrypoint,
  evalWorkerEntrypoint,
  dataURLWorkerEntrypoint,
}));
