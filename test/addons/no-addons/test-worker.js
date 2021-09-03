// Flags: --no-addons

'use strict';

const common = require('../../common');
const assert = require('assert');
const path = require('path');
const { Worker } = require('worker_threads');

const binding = path.resolve(__dirname, `./build/${common.buildType}/binding`);

const expectError = (error) => {
  assert.strictEqual(error.code, 'ERR_DLOPEN_DISABLED');
  assert.strictEqual(
    error.message,
    'Cannot load native addon because loading addons is disabled.'
  );
};

{
  // Flags should be inherited
  const worker = new Worker(`require(${JSON.stringify(binding)})`, {
    eval: true,
  });

  worker.on('error', common.mustCall(expectError));
}

{
  // Explicitly pass `--no-addons`
  const worker = new Worker(`require(${JSON.stringify(binding)})`, {
    eval: true,
    execArgv: ['--no-addons'],
  });

  worker.on('error', common.mustCall(expectError));
}

{
  // If `execArgv` is overwritten it should still fail to load addons
  const worker = new Worker(`require(${JSON.stringify(binding)})`, {
    eval: true,
    execArgv: [],
  });

  worker.on('error', common.mustCall(expectError));
}
