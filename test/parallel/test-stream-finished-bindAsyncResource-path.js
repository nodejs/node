// Flags: --expose-internals
'use strict';

const common = require('../common');
const { Readable, finished } = require('stream');
const { createHook, executionAsyncId } = require('async_hooks');
const assert = require('assert');
const { enabledHooksExist } = require('internal/async_hooks');

// This test verifies that when there are active async hooks, stream.finished() uses
// the bindAsyncResource path

createHook({
  init(asyncId, type, triggerAsyncId) {
    if (type === 'STREAM_END_OF_STREAM') {
      const parentContext = contextMap.get(triggerAsyncId);
      contextMap.set(asyncId, parentContext);
    }
  }
}).enable();

const contextMap = new Map();
const asyncId = executionAsyncId();
contextMap.set(asyncId, 'abc-123');
const readable = new Readable();

finished(readable, common.mustCall(() => {
  const currentAsyncId = executionAsyncId();
  const ctx = contextMap.get(currentAsyncId);
  assert.ok(enabledHooksExist());
  assert.strictEqual(ctx, 'abc-123');
}));

readable.destroy();
