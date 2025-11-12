// Flags: --no-addons

'use strict';

const common = require('../../common');
const assert = require('assert');
const path = require('path');
const { Worker } = require('worker_threads');

const binding = path.resolve(__dirname, `./build/${common.buildType}/binding`);

const assertError = common.mustCall((error) => {
  assert.strictEqual(error.code, 'ERR_DLOPEN_DISABLED');
  assert.strictEqual(
    error.message,
    'Cannot load native addon because loading addons is disabled.',
  );
}, 4);

{
  // Flags should be inherited
  const worker = new Worker(`require(${JSON.stringify(binding)})`, {
    eval: true,
  });

  worker.on('error', assertError);
}

{
  // Should throw when using `process.dlopen` directly
  const worker = new Worker(
    `process.dlopen({ exports: {} }, ${JSON.stringify(binding)});`,
    {
      eval: true,
    },
  );

  worker.on('error', assertError);
}

{
  // Explicitly pass `--no-addons`
  const worker = new Worker(`require(${JSON.stringify(binding)})`, {
    eval: true,
    execArgv: ['--no-addons'],
  });

  worker.on('error', assertError);
}

{
  // If `execArgv` is overwritten it should still fail to load addons
  const worker = new Worker(`require(${JSON.stringify(binding)})`, {
    eval: true,
    execArgv: [],
  });

  worker.on('error', assertError);
}
