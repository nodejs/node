'use strict';
const common = require('../common');
const { Worker, isMainThread } = require('worker_threads');
const EventEmitter = require('events');

if (isMainThread) {
  process.on('warning', common.mustNotCall('unexpected warning'));

  for (let i = 0; i < EventEmitter.defaultMaxListeners; ++i) {
    const worker = new Worker(__filename);

    worker.on('exit', common.mustCall(() => {
      console.log('a'); // This console.log() is part of the test.
    }));
  }
}
