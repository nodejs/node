import { Worker } from 'worker_threads';

const workerURL = new URL('./worker-log.mjs', import.meta.url);

// Spawn two workers
new Worker(workerURL);
new Worker(workerURL);
