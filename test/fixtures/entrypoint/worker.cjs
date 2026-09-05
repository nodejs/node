'use strict';

const { isMainThread, workerData, Worker } = require('node:worker_threads');

if (isMainThread || workerData === 'nested') {
  new Worker(__filename, { workerData: isMainThread ? 'nested' : 'leaf' });
} else {
  console.log(process.entrypoint);
}
