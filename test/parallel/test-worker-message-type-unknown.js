'use strict';

// Check that main thread handles an unknown message type from a worker thread
// as expected.

require('../common');

const assert = require('assert');
const { spawnSync } = require('child_process');

const { Worker } = require('worker_threads');
if (process.argv[2] !== 'spawned') {
  const result = spawnSync(process.execPath,
                           [ '--expose-internals', __filename, 'spawned'],
                           { encoding: 'utf8' });
  assert.ok(result.stderr.includes('Unknown worker message type FHQWHGADS'),
            `Expected error not found in: ${result.stderr}`);
} else {
  new Worker(`
    const { internalBinding } = require('internal/test/binding');
    const { getEnvMessagePort } = internalBinding('worker');
    const messagePort = getEnvMessagePort();
    messagePort.postMessage({ type: 'FHQWHGADS' });
    `, { eval: true });
}
