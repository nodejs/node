'use strict';
const common = require('../../common');
const { isMainThread, Worker } = require('worker_threads');

if (!isMainThread) {
  const addon = require(`./build/${common.buildType}/binding`);

  new addon.MyObject(10);

  // Should not segfault
  throw new Error('exit');
} else {
  const worker = new Worker(__filename);
  worker.on('error', common.mustCall());
}
