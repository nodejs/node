import { Worker } from 'worker_threads';

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  new Worker(new URL(import.meta.url));
  await new Promise(() => {});
} else {
  process.exit()
}
