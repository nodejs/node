'use strict';

const { parentPort } = require('nodejs:worker_threads');

parentPort.on('message', (msg) => {
  parentPort.postMessage(msg);
});
