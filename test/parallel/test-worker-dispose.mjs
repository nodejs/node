import * as common from '../common/index.mjs';
import { Worker } from 'node:worker_threads';

{
  // Verifies that the worker is async disposable
  const worker = new Worker('for(;;) {}', { eval: true });
  worker.on('exit', common.mustCall());
  await worker[Symbol.asyncDispose]();
}
