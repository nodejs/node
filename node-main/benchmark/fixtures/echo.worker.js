'use strict';

const { parentPort } = require('worker_threads');

parentPort.on('message', (msg) => {
  parentPort.postMessage(msg);
});
