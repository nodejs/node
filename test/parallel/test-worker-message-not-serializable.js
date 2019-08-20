'use strict';

// Flags: --expose-internals

// Check that main thread handles unserializable errors in a worker thread as
// expected.

const common = require('../common');

const assert = require('assert');

const { Worker } = require('worker_threads');

const worker = new Worker(`
  const { internalBinding } = require('internal/test/binding');
  const { getEnvMessagePort } = internalBinding('worker');
  const { messageTypes } = require('internal/worker/io');
  const messagePort = getEnvMessagePort();
  messagePort.postMessage({ type: messageTypes.COULD_NOT_SERIALIZE_ERROR });
`, { eval: true });

worker.on('error', common.mustCall((e) => {
  assert.strictEqual(e.code, 'ERR_WORKER_UNSERIALIZABLE_ERROR');
}));
