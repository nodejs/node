'use strict';
const common = require('../common');
const child_process = require('child_process');
const assert = require('assert');
const { Worker, workerData } = require('worker_threads');

// Test for https://github.com/nodejs/node/issues/24947.

if (!workerData && process.argv[2] !== 'child') {
  process.env.SET_IN_PARENT = 'set';
  assert.strictEqual(process.env.SET_IN_PARENT, 'set');

  new Worker(__filename, { workerData: 'runInWorker' })
    .on('exit', common.mustCall(() => {
      // Env vars from the child thread are not set globally.
      assert.strictEqual(process.env.SET_IN_WORKER, undefined);
    }));

  process.env.SET_IN_PARENT_AFTER_CREATION = 'set';

  new Worker(__filename, {
    workerData: 'resetEnv',
    env: { 'MANUALLY_SET': true }
  });

  assert.throws(() => {
    new Worker(__filename, { env: 42 });
  }, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "options.env" property must be of type object or ' +
      'one of undefined, null, or worker_threads.SHARE_ENV. Received type ' +
      'number (42)'
  });
} else if (workerData === 'runInWorker') {
  // Env vars from the parent thread are inherited.
  assert.strictEqual(process.env.SET_IN_PARENT, 'set');
  assert.strictEqual(process.env.SET_IN_PARENT_AFTER_CREATION, undefined);
  process.env.SET_IN_WORKER = 'set';
  assert.strictEqual(process.env.SET_IN_WORKER, 'set');

  assert.throws(
    () => {
      Object.defineProperty(process.env, 'DEFINED_IN_WORKER', {
        value: 42
      });
    },
    {
      code: 'ERR_INVALID_OBJECT_DEFINE_PROPERTY',
      name: 'TypeError',
      message: '\'process.env\' only accepts a configurable, ' +
          'writable, and enumerable data descriptor'
    }
  );


  const { stderr } =
    child_process.spawnSync(process.execPath, [__filename, 'child']);
  assert.strictEqual(stderr.toString(), '', stderr.toString());
} else if (workerData === 'resetEnv') {
  assert.deepStrictEqual(Object.keys(process.env), ['MANUALLY_SET']);
  assert.strictEqual(process.env.MANUALLY_SET, 'true');
} else {
  // Child processes inherit the parent's env, even from Workers.
  assert.strictEqual(process.env.SET_IN_PARENT, 'set');
  assert.strictEqual(process.env.SET_IN_WORKER, 'set');
}
