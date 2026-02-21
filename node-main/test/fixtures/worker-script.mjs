import worker from 'worker_threads';

worker.parentPort.postMessage('Hello, world!');
