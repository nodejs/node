'use strict';

const common = require('../common');

const assert = require('assert');
const util = require('util');
const { Worker, workerData } = require('worker_threads');

if (workerData?.isWorker) {
  assert.throws(() => {
    util.setTraceSigInt(true);
  }, {
    code: 'ERR_WORKER_UNSUPPORTED_OPERATION',
  });
} else {
  const w = new Worker(__filename, { workerData: { isWorker: true } });
  w.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
}
