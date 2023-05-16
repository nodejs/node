'use strict';
const common = require('../../common');

// Refs: https://github.com/nodejs/node/issues/34731
// Refs: https://github.com/nodejs/node/pull/35777
// Refs: https://github.com/nodejs/node/issues/35778

const { Worker, isMainThread } = require('worker_threads');

if (isMainThread) {
  const worker = new Worker(__filename);
  worker.on('error', common.mustNotCall());
} else {
  const { Test } =
    require(`./build/${common.buildType}/test_worker_terminate_finalization`);

  // Spin up thread and call add-on create the right sequence
  // of rerences to hit the case reported in
  // https://github.com/nodejs/node-addon-api/issues/722
  // will crash if run under debug and its not possible to
  // create object in the specific finalizer
  Test(new Object());
}
