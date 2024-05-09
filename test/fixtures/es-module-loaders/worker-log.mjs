import { Worker } from 'worker_threads';
import { foo } from './module-named-exports.mjs';

const workerURL = new URL('./worker-log-again.mjs', import.meta.url);
console.log(foo);

// Spawn two workers
new Worker(workerURL);
new Worker(workerURL);
