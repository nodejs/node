'use strict';

const { parentPort } = require('worker');

parentPort.on('message', (msg) => {
  parentPort.postMessage(msg);
});
