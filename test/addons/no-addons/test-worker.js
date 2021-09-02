'use strict';

const common = require('../../common');
const assert = require('assert');
const path = require('path');
const { Worker } = require('worker_threads');

const binding = path.resolve(__dirname, `./build/${common.buildType}/binding`);

const worker = new Worker(`require(${JSON.stringify(binding)})`, {
  eval: true,
});

worker.on(
  'error',
  common.mustCall((error) => {
    assert.strictEqual(error.code, 'ERR_DLOPEN_FAILED');
    assert.strictEqual(
      error.message,
      'Cannot load native addon because loading addons is disabled.'
    );
  })
);
