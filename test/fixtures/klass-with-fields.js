'use strict';

const {
  parentPort,
  isMainThread
} = require('node:worker_threads');

class Klass {
  numeric = 1234;
  nonNumeric = 'test';
}

globalThis.obj = new Klass();

if (!isMainThread) {
  parentPort.postMessage('ready');
  setInterval(() => {}, 100);
}
