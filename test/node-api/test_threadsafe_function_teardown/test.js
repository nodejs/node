'use strict';

const common = require('../../common');
const assert = require('assert');
const { Worker, isMainThread } = require('worker_threads');

// A worker that exits while a native addon still owns a thread-safe function.
// The TSFN finalizer could trigger napi_env finalization and the addon
// may re-enter the TSFN finalization.
// Refs: https://github.com/nodejs/node/issues/65100

if (isMainThread) {
  const worker = new Worker(__filename);
  worker.on('error', common.mustNotCall());
  worker.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
} else {
  require(`./build/${common.buildType}/binding`);
}
