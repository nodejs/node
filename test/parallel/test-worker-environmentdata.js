'use strict';
// Flags: --expose-internals

require('../common');
const {
  Worker,
  getEnvironmentData,
  setEnvironmentData,
  threadId,
} = require('worker_threads');

const { assignEnvironmentData } = require('internal/worker');

const {
  deepStrictEqual,
  strictEqual,
} = require('assert');

if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  setEnvironmentData('foo', 'bar');
  setEnvironmentData('hello', { value: 'world' });
  setEnvironmentData(1, 2);
  strictEqual(getEnvironmentData(1), 2);
  setEnvironmentData(1); // Delete it, key won't show up in the worker.
  new Worker(__filename);
  setEnvironmentData('hello');  // Delete it. Has no impact on the worker.
} else {
  strictEqual(getEnvironmentData('foo'), 'bar');
  deepStrictEqual(getEnvironmentData('hello'), { value: 'world' });
  strictEqual(getEnvironmentData(1), undefined);
  assignEnvironmentData(undefined); // It won't setup any key.
  strictEqual(getEnvironmentData(undefined), undefined);

  // Recurse to make sure the environment data is inherited
  if (threadId <= 2)
    new Worker(__filename);
}
