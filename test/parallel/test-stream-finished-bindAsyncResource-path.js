// Flags: --expose-internals
'use strict';

const common = require('../common');
const { Readable, finished } = require('stream');
const { createHook, executionAsyncId } = require('async_hooks');
const { strictEqual } = require('assert');
const internalAsyncHooks = require('internal/async_hooks');

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
  strictEqual(internalAsyncHooks.getHookArrays()[0].length > 0,
              true, 'Should have active user async hook');
  strictEqual(ctx, 'abc-123', 'Context should be preserved');
}));

readable.destroy();
