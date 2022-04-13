'use strict';

const common = require('../common');
const { Worker } = require('worker_threads');

common.skipIfInspectorDisabled();

if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  new Worker(__filename);
  return;
}

const assert = require('assert');
const { Session } = require('inspector');

const session = new Session();
session.connect();
session.post('NodeTracing.start', {
  traceConfig: { includedCategories: ['node.perf'] }
}, common.mustCall((err) => {
  assert.deepStrictEqual(err, {
    code: -32000,
    message:
      'Tracing properties can only be changed through main thread sessions'
  });
}));
session.disconnect();
