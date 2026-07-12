import { isMainThread, Worker } from 'node:worker_threads';

const registeredRefs = [];

const ref = { foo: 'foo' };
registeredRefs.push(ref);

if (isMainThread) {
  process.finalization.register(ref, () => {
    process.stdout.write('shutdown on main thread\n');
  });

  const worker = new Worker(import.meta.filename);

  worker.on('error', (err) => {
    // Referencing `registeredRefs` here to avoid `ref` being GCed before the worker exits.
    console.log(registeredRefs);
    throw err;
  });

  worker.postMessage('ping');
} else {
  process.finalization.register(ref, () => {
    process.stdout.write('shutdown on worker\n');
  });
}
