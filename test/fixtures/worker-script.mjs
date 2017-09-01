import worker from 'worker';

worker.parentPort.postMessage('Hello, world!');
