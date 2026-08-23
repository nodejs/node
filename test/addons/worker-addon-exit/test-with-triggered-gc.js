'use strict';
const common = require('../../common');
const { isMainThread, Worker } = require('worker_threads');

if (!isMainThread) {
  const addon = require(`./build/${common.buildType}/binding`);


  // Create some garbage
  const arr = [];
  for (let i = 0; i < 1e5; i++) arr.push(`${i}`.repeat(100));

  new addon.MyObject(10);

  // Should not segfault
  throw new Error('exit');
} else {
  const worker = new Worker(__filename);
  worker.on('error', common.mustCall());
}
