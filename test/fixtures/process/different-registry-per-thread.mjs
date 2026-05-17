import { isMainThread, Worker } from 'node:worker_threads';

const registeredRefs = [];

const ref = { foo: 'foo' };
registeredRefs.push(ref);

process.on('exit', () => {
  // Keep `ref` strongly reachable until exit so the per-thread finalization
  // callbacks are tested without depending on GC timing.
  registeredRefs.length;
});

if (isMainThread) {
  process.finalization.register(ref, () => {
    process.stdout.write('shutdown on main thread\n');
  });

  const worker = new Worker(import.meta.filename);

  worker.on('error', (err) => {
    console.log(registeredRefs);
    throw err;
  });

  worker.postMessage('ping');
} else {
  process.finalization.register(ref, () => {
    process.stdout.write('shutdown on worker\n');
  });
}
